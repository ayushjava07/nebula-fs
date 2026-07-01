#pragma once

#include "../Config.hpp"
#include "../Types.hpp"

#include <cstdint>
#include <cstddef>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

namespace nebula {
namespace network {

/// A simple thread pool for processing tasks concurrently.
///
/// Manages a fixed set of worker threads that pull
/// tasks from a shared queue.
class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// Enqueue a task for execution by worker threads.
    void enqueueTask(Task* task);

    /// Enqueue a task by copying the functor.
    void enqueue(std::function<void()> func);

    /// Get the number of worker threads.
    [[nodiscard]] size_t threadCount() const noexcept { return workers_.size(); }

    /// Check if the pool is still running.
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

    /// Wait until all queued tasks are complete.
    void waitAll();

    /// Get the number of pending tasks.
    [[nodiscard]] size_t pendingTasks() const noexcept;

private:
    std::vector<std::thread> workers_;
    std::queue<Task*> taskQueue_;
    std::queue<Task*> highPriorityQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
    std::atomic<size_t> activeTasks_{0};

    void workerLoop();
};

} // namespace network
} // namespace nebula
