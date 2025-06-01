// include/thread_pool.hpp
#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future> // For std::future, std::packaged_task
#include <functional> // For std::function
#include <stdexcept> // For std::runtime_error

namespace FileManager {
namespace Concurrency {

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();

    // Enqueue a task to be executed by a thread in the pool.
    // Returns a std::future that will hold the result of the task.
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;

private:
    // Need to keep track of threads so we can join them
    std::vector<std::thread> workers;
    // The queue of tasks
    std::queue<std::function<void()>> tasks;

    // Synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop_all; // Flag to signal threads to stop
};

// --- Template method implementation (usually in .hpp for templates) ---
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    // Define the return type of the function f
    using return_type = typename std::result_of<F(Args...)>::type;

    // Create a packaged_task to wrap the function and its arguments
    // std::bind is used to bind arguments to the function
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    // Get a future associated with the task
    std::future<return_type> res = task->get_future();

    {
        // Acquire lock to push task to queue
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Don't allow enqueueing after stopping the pool
        if (stop_all)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        // Push the task into the queue as a void function
        tasks.emplace([task]() { (*task)(); });
    }
    // Notify one waiting thread that a new task is available
    condition.notify_one();
    return res;
}

} // namespace Concurrency
} // namespace FileManager