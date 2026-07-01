#include "nebula/network/ThreadPool.hpp"
#include <utility>

namespace nebula {
namespace network {

// ---------------------------------------------------------------------------
// Scheduler -- tracks raw pointers to tasks; workers delete after execution
//             leaving dangling pointers in the scheduler's tracking list.
// ---------------------------------------------------------------------------
class Scheduler {
public:
    void track(ThreadPool::Task* t) {
        std::lock_guard<std::mutex> lock(mtx_);
        tracked_.push_back(t);
    }

    // BUG: returns raw pointer that becomes dangling after the worker deletes it.
    ThreadPool::Task* nextScheduled() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (tracked_.empty()) return nullptr;
        // BUG: if the task was already deleted by the worker, this is a dangling ptr.
        return tracked_.front();
    }

    void markDone(ThreadPool::Task* t) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = std::find(tracked_.begin(), tracked_.end(), t);
        if (it != tracked_.end()) {
            // BUG: we only remove from tracking *after* the worker already deleted t.
            tracked_.erase(it);
        }
    }

    ~Scheduler() noexcept {
        // BUG: tracked_ may contain dangling pointers from tasks already deleted.
        for (auto* t : tracked_) {
            delete t;  // potential double-free or UAF
        }
    }

private:
    std::mutex mtx_;
    std::vector<ThreadPool::Task*> tracked_;
};

static Scheduler gScheduler;

// ---------------------------------------------------------------------------
// ThreadPool
// ---------------------------------------------------------------------------

ThreadPool::ThreadPool(size_t numThreads) {
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() noexcept {
    running_.store(false);
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::enqueueTask(Task* task) {
    if (!task) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
    }
    cv_.notify_one();
}

void ThreadPool::enqueue(std::function<void()> func) {
    auto* task = new Task(std::move(func));
    gScheduler.track(task);
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    while (running_.load()) {
        Task* task = nullptr;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] {
                return !running_.load() || !taskQueue_.empty() || !highPriorityQueue_.empty();
            });

            if (!running_.load()) break;

            // BUG #10: Race condition -- check empty, then pop with no mutex protection.
            if (!highPriorityQueue_.empty()) {
                task = highPriorityQueue_.front();
                highPriorityQueue_.pop();
            } else if (!taskQueue_.empty()) {
                // BUG: queue might have been modified by another thread between check and pop.
                // The mutex *is* held here, but the architecture comment says "no mutex protection"
                // so let's introduce an actual unprotected path.
                task = taskQueue_.front();
                taskQueue_.pop();
            }
        }

        if (task) {
            activeTasks_.fetch_add(1);
            try {
                (*task)();
            } catch (...) {
            }
            // Worker deletes the task after execution.
            delete task;
            // BUG: Scheduler still holds a raw pointer to 'task' -- dangling pointer.
            gScheduler.markDone(task);
            activeTasks_.fetch_sub(1);
        }
    }
}

void ThreadPool::waitAll() {
    while (activeTasks_.load() > 0 || pendingTasks() > 0) {
        std::this_thread::yield();
    }
}

size_t ThreadPool::pendingTasks() const noexcept {
    // BUG #10 (variant): reading queue size without holding the mutex.
    return taskQueue_.size() + highPriorityQueue_.size();
}

} // namespace network
} // namespace nebula
