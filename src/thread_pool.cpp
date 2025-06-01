// src/thread_pool.cpp
#include "thread_pool.hpp"
#include <iostream> // For logging (can be replaced)

namespace FileManager
{
    namespace Concurrency
    {

        ThreadPool::ThreadPool(size_t num_threads) : stop_all(false)
        {
            if (num_threads == 0)
            {
                throw std::runtime_error("ThreadPool cannot be initialized with 0 threads.");
            }
            for (size_t i = 0; i < num_threads; ++i)
            {
                workers.emplace_back(
                    [this] { // The worker thread function
                        for (;;)
                        {
                            std::function<void()> task;
                            {
                                std::unique_lock<std::mutex> lock(this->queue_mutex);
                                // Wait until there's a task or the pool is stopping
                                this->condition.wait(lock,
                                                     [this]
                                                     { return this->stop_all || !this->tasks.empty(); });

                                // If stopping and no more tasks, exit the loop
                                if (this->stop_all && this->tasks.empty())
                                    return;

                                // Get the task from the front of the queue
                                task = std::move(this->tasks.front());
                                this->tasks.pop();
                            }
                            // Execute the task outside the lock
                            task();
                        }
                    });
            }
            std::cout << "ThreadPool initialized with " << num_threads << " threads." << std::endl;
        }

        ThreadPool::~ThreadPool()
        {
            {
                // Acquire lock to set the stop flag
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop_all = true;
            }
            // Notify all waiting threads to wake up and check the stop flag
            condition.notify_all();
            // Join all worker threads
            for (std::thread &worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
            std::cout << "ThreadPool destroyed." << std::endl;
        }

    } // namespace Concurrency
} // namespace FileManager