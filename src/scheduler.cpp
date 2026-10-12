#include "mini_judge/scheduler.hpp"

#include <stdexcept>
#include <utility>

namespace mini_judge {

namespace {
thread_local const TaskScheduler* current_scheduler = nullptr;

std::size_t QueueCapacity(int jobs, std::size_t capacity) {
    if (jobs < 1 || jobs > TaskScheduler::kMaxWorkers) {
        throw std::invalid_argument("jobs must be between 1 and 256");
    }
    return capacity == 0 ? static_cast<std::size_t>(jobs) * 4 : capacity;
}
}  // namespace

TaskScheduler::TaskScheduler(int jobs, std::size_t queue_capacity)
    : queue_capacity_(QueueCapacity(jobs, queue_capacity)) {
    workers_.reserve(static_cast<std::size_t>(jobs));
    try {
        for (int i = 0; i < jobs; ++i) {
            workers_.emplace_back([this]() { WorkerLoop(); });
        }
    } catch (...) {
        // Join partially created threads before their vector is destroyed.
        Cancel();
        for (auto& worker : workers_) {
            worker.join();
        }
        throw;
    }
}

TaskScheduler::~TaskScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    space_cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void TaskScheduler::Submit(Task task) {
    if (current_scheduler == this) {
        throw std::logic_error("A scheduler task cannot submit to its own pool");
    }
    if (!task) {
        throw std::invalid_argument("Cannot submit an empty task");
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        space_cv_.wait(lock, [this]() { return stopping_ || tasks_.size() < queue_capacity_; });
        if (stopping_) {
            throw std::runtime_error("Scheduler is stopped");
        }
        tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
}

void TaskScheduler::Wait() {
    if (current_scheduler == this) {
        throw std::logic_error("A scheduler task cannot wait for its own pool");
    }
    std::exception_ptr failure;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        finished_cv_.wait(lock, [this]() { return tasks_.empty() && active_workers_ == 0; });
        failure = std::exchange(first_failure_, nullptr);
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void TaskScheduler::Cancel() {
    // Release user captures outside the lock.
    std::deque<Task> discarded;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        tasks_.swap(discarded);
    }
    cv_.notify_all();
    space_cv_.notify_all();
    finished_cv_.notify_all();
}

void TaskScheduler::WorkerLoop() {
    current_scheduler = this;
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) {
                current_scheduler = nullptr;
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop_front();
            ++active_workers_;
        }

        space_cv_.notify_one();

        std::exception_ptr failure;
        try {
            task();
        } catch (...) {
            failure = std::current_exception();
        }
        // Release captures before advertising completion to Wait().
        task = nullptr;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (failure && !first_failure_) {
                first_failure_ = failure;
            }
            --active_workers_;
            if (tasks_.empty() && active_workers_ == 0) {
                finished_cv_.notify_all();
            }
        }
    }
}

}  // namespace mini_judge
