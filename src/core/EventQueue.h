#pragma once

#include <queue>
#include <mutex>
#include <functional>

namespace yt {

class EventQueue {
public:
    using Action = std::function<void()>;

    static EventQueue& instance() {
        static EventQueue s_instance;
        return s_instance;
    }

    void post(Action action) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_actions.push(std::move(action));
    }

    void processPending() {
        std::queue<Action> currentBatch;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            currentBatch.swap(m_actions);
        }

        while (!currentBatch.empty()) {
            auto& action = currentBatch.front();
            if (action) {
                action();
            }
            currentBatch.pop();
        }
    }

    bool hasPending() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_actions.empty();
    }

private:
    EventQueue() = default;
    mutable std::mutex m_mutex;
    std::queue<Action> m_actions;
};

} // namespace yt
