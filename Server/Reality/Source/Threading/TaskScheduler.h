#ifndef MXOSIM_TASKSCHEDULER_H
#define MXOSIM_TASKSCHEDULER_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <type_traits>

class TaskScheduler
{
public:
    static TaskScheduler& Get()
    {
        static TaskScheduler s_instance;
        return s_instance;
    }

    void Initialize(size_t threads = 0)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_initialized)
            return;

        if (threads == 0)
        {
            threads = std::thread::hardware_concurrency();
            if (threads < 4)
                threads = 4;
        }

        m_stop = false;
        m_workers.reserve(threads);
        for (size_t i = 0; i < threads; ++i)
        {
            m_workers.emplace_back([this, i] {
                while (true)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->m_queueMutex);
                        this->m_cv.wait(lock, [this] {
                            return this->m_stop || !this->m_tasks.empty();
                        });

                        if (this->m_stop && this->m_tasks.empty())
                            return;

                        task = std::move(this->m_tasks.front());
                        this->m_tasks.pop();
                    }

                    try {
                        task();
                    } catch (...) {
                        // Suppress task-level exceptions from killing worker threads
                    }
                }
            });
        }
        m_initialized = true;
    }

    template<class F, class... Args>
    auto Enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        if (!m_initialized)
        {
            Initialize();
        }

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop)
                throw std::runtime_error("TaskScheduler: enqueue on stopped scheduler");

            m_tasks.emplace([task]() { (*task)(); });
        }
        m_cv.notify_one();
        return res;
    }

    void ParallelFor(size_t start, size_t end, const std::function<void(size_t)>& fn, size_t chunkSize = 32)
    {
        if (start >= end)
            return;

        if (!m_initialized)
            Initialize();

        size_t total = end - start;
        if (total <= chunkSize || m_workers.empty())
        {
            for (size_t i = start; i < end; ++i)
                fn(i);
            return;
        }

        size_t numChunks = (total + chunkSize - 1) / chunkSize;
        std::atomic<size_t> completed(0);
        std::vector<std::future<void>> futures;
        futures.reserve(numChunks);

        for (size_t c = 0; c < numChunks; ++c)
        {
            size_t cStart = start + c * chunkSize;
            size_t cEnd = std::min(end, cStart + chunkSize);

            futures.push_back(Enqueue([&fn, cStart, cEnd]() {
                for (size_t i = cStart; i < cEnd; ++i)
                {
                    fn(i);
                }
            }));
        }

        for (auto& fut : futures)
        {
            fut.wait();
        }
    }

    size_t GetWorkerCount() const
    {
        return m_workers.size();
    }

    void Shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (!m_initialized || m_stop)
                return;
            m_stop = true;
        }
        m_cv.notify_all();
        for (std::thread &worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }
        m_workers.clear();
        m_initialized = false;
    }

    ~TaskScheduler()
    {
        Shutdown();
    }

private:
    TaskScheduler() : m_stop(false), m_initialized(false) {}

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop;
    std::atomic<bool> m_initialized;
};

#define sTaskScheduler TaskScheduler::Get()

#endif // MXOSIM_TASKSCHEDULER_H
