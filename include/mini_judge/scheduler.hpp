#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace mini_judge {

class TaskScheduler {
public:
    using Task = std::function<void()>;

    static constexpr int kMaxWorkers = 256;

    // A zero capacity chooses 4 * jobs queued tasks, in addition to running tasks.
    explicit TaskScheduler(int jobs, std::size_t queue_capacity = 0);
    ~TaskScheduler();

    // Controller-thread operations; calling Submit/Wait from this pool's task
    // throws instead of risking a full-queue or self-wait deadlock.
    void Submit(Task task);
    // Drains accepted tasks and rethrows the first task failure, once.
    void Wait();
    // Discards queued work, wakes blocked producers and rejects new submissions.
    // Already running tasks finish normally. Wait/destruction joins that work.
    void Cancel();

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::deque<Task> tasks_;
    const std::size_t queue_capacity_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finished_cv_;
    std::condition_variable space_cv_;
    bool stopping_ = false;
    std::size_t active_workers_ = 0;
    std::exception_ptr first_failure_;
};

}  // namespace mini_judge
