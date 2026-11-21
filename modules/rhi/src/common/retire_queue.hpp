#pragma once

// Backend-agnostic deferred destruction of GPU objects (ARCHITECTURE §5).
//
// When the CPU owner of a GPU object goes away, the object's destructor is pushed here together with the
// per-queue timeline values that were last submitted at that moment. The destructor runs once every queue
// has completed (at least) those values, i.e. once no GPU work that could reference the object is in flight.

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace byte::render::rhi::detail {

template <size_t QueueCount>
using QueueValues = std::array<uint64_t, QueueCount>;

// True if every queue has completed at least the required value.
template <size_t QueueCount>
[[nodiscard]] constexpr bool is_reached(const QueueValues<QueueCount>& required,
                                        const QueueValues<QueueCount>& completed) noexcept {
    for (size_t queue = 0; queue < QueueCount; ++queue) {
        if (completed[queue] < required[queue]) {
            return false;
        }
    }
    return true;
}

// Thread-safe FIFO of pending destructors. Destructors always run outside the internal lock, so they may
// retire further objects.
//
// Entries are processed in push order and processing stops at the first entry that is not ready yet.
// Values are non-decreasing in push order in practice; if concurrent pushes interleave, a later-ready entry
// can delay an earlier-ready one behind it. That is conservative (never destroys too early), not incorrect.
template <size_t QueueCount>
class RetireQueue {
public:
    using Values = QueueValues<QueueCount>;
    using Destroy = std::function<void()>;

    void push(const Values& retire_after, Destroy destroy) {
        const std::scoped_lock lock{m_mutex};
        m_entries.push_back(Entry{.retire_after = retire_after, .destroy = std::move(destroy)});
    }

    // Runs every ready destructor (in push order) and returns how many ran.
    size_t collect(const Values& completed) {
        std::vector<Destroy> ready;
        {
            const std::scoped_lock lock{m_mutex};
            while (!m_entries.empty() && is_reached(m_entries.front().retire_after, completed)) {
                ready.push_back(std::move(m_entries.front().destroy));
                m_entries.pop_front();
            }
        }
        for (Destroy& destroy : ready) {
            destroy();
        }
        return ready.size();
    }

    // Runs every pending destructor regardless of GPU progress. Only valid when the GPU is idle.
    size_t drain() {
        size_t total = 0;
        // Destructors may retire more objects; keep going until nothing is left.
        while (true) {
            std::deque<Entry> entries;
            {
                const std::scoped_lock lock{m_mutex};
                entries.swap(m_entries);
            }
            if (entries.empty()) {
                return total;
            }
            for (Entry& entry : entries) {
                entry.destroy();
            }
            total += entries.size();
        }
    }

    [[nodiscard]] size_t size() const {
        const std::scoped_lock lock{m_mutex};
        return m_entries.size();
    }

private:
    struct Entry {
        Values retire_after{};
        Destroy destroy;
    };

    mutable std::mutex m_mutex;
    std::deque<Entry> m_entries;
};

} // namespace byte::render::rhi::detail
