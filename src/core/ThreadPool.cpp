#include "core/ThreadPool.h"
#include "core/Logger.h"

namespace yt {

ThreadPool::ThreadPool(size_t numThreads) {
    LOG_INFO("Initializing ThreadPool with " + std::to_string(numThreads) + " worker threads");
    for (size_t i = 0; i < numThreads; ++i) {
        m_workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->m_queueMutex);
                    this->m_cv.wait(lock, [this]() {
                        return this->m_stop || !this->m_tasks.empty();
                    });

                    if (this->m_stop && this->m_tasks.empty()) {
                        return;
                    }

                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }

                try {
                    task();
                } catch (const std::exception& ex) {
                    LOG_ERROR("Exception in thread pool task: " + std::string(ex.what()));
                } catch (...) {
                    LOG_ERROR("Unknown exception in thread pool task");
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        if (m_stop) return;
        m_stop = true;
    }
    m_cv.notify_all();

    for (std::thread& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

} // namespace yt
