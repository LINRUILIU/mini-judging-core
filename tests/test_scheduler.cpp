#include "mini_judge/scheduler.hpp"
#include "test_support.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

using namespace mini_judge;
using namespace std::chrono_literals;

namespace {

void TestValidationAndException() {
    TestExpectThrows([]() { TaskScheduler scheduler(0); });
    TestExpectThrows([]() { TaskScheduler scheduler(-1); });
    TestExpectThrows([]() { TaskScheduler scheduler(257); });

    std::atomic<int> completed{0};
    TaskScheduler scheduler(1);
    scheduler.Submit([]() { throw std::runtime_error("fixture task error"); });
    scheduler.Submit([&completed]() { ++completed; });
    TestExpectThrows([&scheduler]() { scheduler.Wait(); });
    TEST_CHECK(completed.load() == 1);
    scheduler.Wait(); // The exception is reported once.
    TestExpectThrows([&scheduler]() { scheduler.Submit({}); });
}

void TestFifoAndBackpressure() {
    std::promise<void> release_first;
    auto gate = release_first.get_future().share();
    std::promise<void> first_started;
    auto started = first_started.get_future();
    std::vector<int> order;
    std::mutex order_mutex;
    TaskScheduler scheduler(1, 2);

    scheduler.Submit([&]() {
        first_started.set_value();
        gate.wait();
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(1);
    });
    const bool first_ready = started.wait_for(2s) == std::future_status::ready;
    if (!first_ready) {
        release_first.set_value();
        TEST_CHECK(first_ready);
    }
    scheduler.Submit([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(2);
    });
    scheduler.Submit([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(3);
    });

    std::promise<void> submit_started;
    auto submit_ready = submit_started.get_future();
    auto fourth = std::async(std::launch::async, [&]() {
        submit_started.set_value();
        scheduler.Submit([&]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back(4);
        });
    });
    const bool producer_started = submit_ready.wait_for(2s) == std::future_status::ready;
    const bool was_blocked = fourth.wait_for(100ms) == std::future_status::timeout;
    release_first.set_value();
    TEST_CHECK(producer_started);
    TEST_CHECK(was_blocked);
    TEST_CHECK(fourth.wait_for(2s) == std::future_status::ready);
    fourth.get();
    scheduler.Wait();
    TEST_CHECK(order == std::vector<int>({1, 2, 3, 4}));
}

void TestCancelWakesSubmit() {
    std::promise<void> release_first;
    auto gate = release_first.get_future().share();
    std::promise<void> first_started;
    auto started = first_started.get_future();
    std::atomic<int> queued_ran{0};
    TaskScheduler scheduler(1, 1);
    scheduler.Submit([&]() {
        first_started.set_value();
        gate.wait();
    });
    const bool first_ready = started.wait_for(2s) == std::future_status::ready;
    if (!first_ready) {
        release_first.set_value();
        TEST_CHECK(first_ready);
    }
    scheduler.Submit([&queued_ran]() { ++queued_ran; });
    std::promise<void> submit_started;
    auto submit_ready = submit_started.get_future();
    auto blocked_submit = std::async(std::launch::async, [&]() {
        submit_started.set_value();
        scheduler.Submit([&queued_ran]() { ++queued_ran; });
    });
    const bool producer_started = submit_ready.wait_for(2s) == std::future_status::ready;
    const bool was_blocked = blocked_submit.wait_for(100ms) == std::future_status::timeout;
    scheduler.Cancel();
    release_first.set_value();
    TEST_CHECK(producer_started);
    TEST_CHECK(was_blocked);
    TEST_CHECK(blocked_submit.wait_for(2s) == std::future_status::ready);
    TestExpectThrows([&blocked_submit]() { blocked_submit.get(); });
    scheduler.Wait();
    TEST_CHECK(queued_ran.load() == 0);
    TestExpectThrows([&scheduler]() { scheduler.Submit([]() {}); });
}

} // namespace

int main() {
    try {
        TestValidationAndException();
        TestFifoAndBackpressure();
        TestCancelWakesSubmit();
        std::cout << "mini_judge_scheduler_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
