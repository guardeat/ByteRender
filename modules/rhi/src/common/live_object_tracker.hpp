#pragma once

// Backend-agnostic bookkeeping of live child objects (Instance -> Devices, Device -> resources), used to report
// leaks by type and debug name when a parent is destroyed while children still exist.

#include <bytee/core/assert.hpp>
#include <bytee/core/log.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

namespace bytee::render::rhi::detail {

// Thread-safe. Not on hot paths: only object creation and destruction touch it.
class LiveObjectTracker {
public:
    using Id = uint64_t;

private:
    struct Entry {
        std::string type;
        std::string name;
    };

    mutable std::mutex mutex_;
    std::map<Id, Entry> objects_; // ordered by creation: deterministic leak reports
    Id next_id_ = 1;

public:
    [[nodiscard]] Id add(std::string_view type, std::string_view name) {
        const std::scoped_lock lock{mutex_};
        const Id id = next_id_++;
        objects_.emplace(id, Entry{.type = std::string{type}, .name = std::string{name}});
        return id;
    }

    void remove(Id id) {
        const std::scoped_lock lock{mutex_};
        [[maybe_unused]] const std::size_t erased = objects_.erase(id);
        BYTEE_ASSERT(erased == 1, "LiveObjectTracker::remove: unknown id {}", id);
    }

    [[nodiscard]] std::size_t count() const {
        const std::scoped_lock lock{mutex_};
        return objects_.size();
    }

    // Logs one error per live object, in creation order, and returns how many there were.
    std::size_t report_leaks(core::Logger& logger, std::string_view owner) const {
        const std::scoped_lock lock{mutex_};
        for (const auto& [id, entry] : objects_) {
            logger.error("{} destroyed while {} '{}' is still alive", owner, entry.type, entry.name);
        }
        return objects_.size();
    }
};

} // namespace bytee::render::rhi::detail
