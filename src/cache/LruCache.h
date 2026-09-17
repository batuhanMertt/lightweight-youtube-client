#pragma once

#include <list>
#include <unordered_map>
#include <mutex>
#include <cstddef>
#include <optional>

namespace yt {

template <typename Key, typename Value>
class LruCache {
public:
    using KeyValuePair = std::pair<Key, Value>;
    using ListIterator = typename std::list<KeyValuePair>::iterator;

    explicit LruCache(size_t maxCapacity)
        : m_maxCapacity(maxCapacity) {}

    void put(const Key& key, const Value& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_items.erase(it->second);
            m_items.push_front(std::make_pair(key, value));
            it->second = m_items.begin();
            return;
        }

        if (m_items.size() >= m_maxCapacity) {
            auto last = m_items.end();
            --last;
            m_map.erase(last->first);
            m_items.pop_back();
        }

        m_items.push_front(std::make_pair(key, value));
        m_map[key] = m_items.begin();
    }

    std::optional<Value> get(const Key& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_map.find(key);
        if (it == m_map.end()) {
            return std::nullopt;
        }
        // Move accessed item to front of LRU
        m_items.splice(m_items.begin(), m_items, it->second);
        return it->second->second;
    }

    bool contains(const Key& key) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_map.find(key) != m_map.end();
    }

    void remove(const Key& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_map.find(key);
        if (it != m_map.end()) {
            m_items.erase(it->second);
            m_map.erase(it);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items.clear();
        m_map.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_items.size();
    }

    size_t capacity() const {
        return m_maxCapacity;
    }

private:
    size_t m_maxCapacity;
    mutable std::mutex m_mutex;
    std::list<KeyValuePair> m_items;
    std::unordered_map<Key, ListIterator> m_map;
};

} // namespace yt
