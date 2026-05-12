#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace mini_judge {

class TaskScheduler {
public:
    using Task = std::function<void()>;

    explicit TaskScheduler(int jobs);
    ~TaskScheduler();

    void Submit(Task task);
    void Wait();

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::vector<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finished_cv_;
    bool stopping_ = false;
    std::size_t active_workers_ = 0;
};

}  // namespace mini_judge
