#pragma once

// Backend-agnostic bookkeeping of live RHI objects, used to report leaks (by type and debug name)
// when a parent object is destroyed while children still exist.

#include <byte/core/assert.hpp>
#include <byte/core/log.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace byte::render::rhi::detail {

class LiveObjectTracker {
public:
    using Id = uint64_t;

    [[nodiscard]] Id add(std::string_view type, std::string_view name) {
        const std::scoped_lock lock{m_mutex};
        const Id id = m_next_id++;
        m_objects.emplace(id, Entry{.type = std::string{type}, .name = std::string{name}});
        return id;
    }

    void remove(Id id) {
        const std::scoped_lock lock{m_mutex};
        [[maybe_unused]] const size_t erased = m_objects.erase(id);
        BYTE_RENDER_ASSERT(erased == 1, "unknown live object id {}", id);
    }

    [[nodiscard]] size_t count() const {
        const std::scoped_lock lock{m_mutex};
        return m_objects.size();
    }

    // Logs one error per live object (in creation order) and returns how many there were.
    size_t report_leaks(core::Logger& logger, std::string_view owner) const {
        const std::scoped_lock lock{m_mutex};
        for (const auto& [id, entry] : m_objects) {
            logger.error("{} destroyed while {} '{}' is still alive", owner, entry.type, entry.name);
        }
        return m_objects.size();
    }

private:
    struct Entry {
        std::string type;
        std::string name;
    };

    mutable std::mutex m_mutex;
    std::map<Id, Entry> m_objects;
    Id m_next_id = 1;
};

} // namespace byte::render::rhi::detail
