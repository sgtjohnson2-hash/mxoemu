#ifndef MXOEMU_ASYNC_DATABASE_H
#define MXOEMU_ASYNC_DATABASE_H

#include "../Common.h"
#include "../Singleton.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <string>

class PreparedStatement;

struct AsyncDBTask {
    PreparedStatement* stmt = nullptr;
    std::string sql = "";
};

class AsyncDatabase : public Singleton<AsyncDatabase>{
public:
    AsyncDatabase();
    ~AsyncDatabase();

    // Starts the background worker thread
    void Initialize();

    // Stops the worker thread safely, flushing remaining queries
    void Shutdown();

    // Pushes a prepared statement into the queue (fire and forget).
    // The AsyncDatabase takes ownership and will delete the stmt when done.
    void Enqueue(PreparedStatement* stmt);

    // Pushes a raw SQL string into the queue.
    void Enqueue(const std::string& sql);

    // Get the current number of pending queries
    size_t GetQueueSize();

private:
    void WorkerLoop();

    std::queue<AsyncDBTask> m_queue;
    std::thread m_workerThread;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::atomic<bool> m_running;
};

#define sAsyncDatabase Singleton<AsyncDatabase>::getSingleton()

#endif // MXOEMU_ASYNC_DATABASE_H
