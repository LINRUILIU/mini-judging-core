#include "mini_judge/scheduler.hpp"

#include <stdexcept>

namespace mini_judge {

TaskScheduler::TaskScheduler(int jobs) {
    const int worker_count = jobs > 0 ? jobs : 1;
    workers_.reserve(static_cast<std::size_t>(worker_count));
    for (int i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this]() { WorkerLoop(); });
    }
}

TaskScheduler::~TaskScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void TaskScheduler::Submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(std::move(task));
    }
    cv_.notify_one();
}

void TaskScheduler::Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    finished_cv_.wait(lock, [this]() { return tasks_.empty() && active_workers_ == 0; });
}

void TaskScheduler::WorkerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.back());
            tasks_.pop_back();
            ++active_workers_;
        }

        task();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            --active_workers_;
            if (tasks_.empty() && active_workers_ == 0) {
                finished_cv_.notify_all();
            }
        }
    }
}

}  // namespace mini_judge
