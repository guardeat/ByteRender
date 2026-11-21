#pragma once

#include <cstdint>
#include <string_view>

namespace bytee::render::rhi {

// GPU queues of a Device. v1 has one universal queue (graphics + compute + transfer); dedicated async compute and
// transfer queues can be added later without changing the shape of this API.
enum class QueueType : uint8_t {
    Graphics,
};

[[nodiscard]] constexpr std::string_view to_string(QueueType type) noexcept {
    switch (type) {
    case QueueType::Graphics: return "Graphics";
    }
    return "Unknown";
}

// A point in a queue's submission stream. Complete once the GPU has finished all work submitted to that queue up to
// and including it. Submissions return SyncPoints; a default SyncPoint is always complete.
struct SyncPoint {
    QueueType queue = QueueType::Graphics;
    uint64_t value = 0;

    friend constexpr bool operator==(const SyncPoint&, const SyncPoint&) noexcept = default;
};

} // namespace bytee::render::rhi
