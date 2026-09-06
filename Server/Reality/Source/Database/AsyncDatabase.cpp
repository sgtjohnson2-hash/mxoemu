#include "AsyncDatabase.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "../Log.h"

createFileSingleton(AsyncDatabase);

AsyncDatabase::AsyncDatabase() : m_running(false)
{
}

AsyncDatabase::~AsyncDatabase()
{
    Shutdown();
}

void AsyncDatabase::Initialize()
{
    if (m_running) return;

    m_running = true;
    m_workerThread = std::thread(&AsyncDatabase::WorkerLoop, this);
    INFO_LOG("AsyncDatabase Worker Thread Started.");
}

void AsyncDatabase::Shutdown()
{
    if (!m_running) return;

    INFO_LOG("AsyncDatabase: Shutting down, flushing query queue...");
    m_running = false;
    m_cond.notify_all();

    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }
}

void AsyncDatabase::Enqueue(PreparedStatement* stmt)
{
    if (!stmt) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        AsyncDBTask query;
        query.stmt = stmt;
        m_queue.push(query);
    }
    m_cond.notify_one();
}

void AsyncDatabase::Enqueue(const std::string& sql)
{
    if (sql.empty()) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        AsyncDBTask query;
        query.sql = sql;
        m_queue.push(query);
    }
    m_cond.notify_one();
}

size_t AsyncDatabase::GetQueueSize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void AsyncDatabase::WorkerLoop()
{
    while (m_running || !m_queue.empty())
    {
        AsyncDBTask query;
        bool hasQuery = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this]() { return !m_queue.empty() || !m_running; });

            if (!m_queue.empty())
            {
                query = m_queue.front();
                m_queue.pop();
                hasQuery = true;
            }
        }

        if (hasQuery)
        {
            try {
                if (query.stmt)
                {
                    sDatabase.ExecutePrepared(query.stmt);
                    delete query.stmt;
                }
                else if (!query.sql.empty())
                {
                    sDatabase.Execute(query.sql);
                }
            } catch (const std::exception& e) {
                WARNING_LOG(format("AsyncDatabase: SQL Execution Failed! Error: %1%") % e.what());
                // If it was a stmt, we still need to delete it to prevent memory leak
                if (query.stmt) delete query.stmt;
            } catch (...) {
                WARNING_LOG("AsyncDatabase: Unknown Exception during SQL execution.");
                if (query.stmt) delete query.stmt;
            }
        }
    }
}
