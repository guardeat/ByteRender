#pragma once

#include <byte/core/version.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace byte::render::rhi {

enum class AdapterType : uint8_t {
    Unknown,
    Discrete,
    Integrated,
    Virtual,
    Cpu,
};

constexpr std::string_view to_string(AdapterType type) noexcept {
    switch (type) {
    case AdapterType::Unknown: return "Unknown";
    case AdapterType::Discrete: return "Discrete";
    case AdapterType::Integrated: return "Integrated";
    case AdapterType::Virtual: return "Virtual";
    case AdapterType::Cpu: return "Cpu";
    }
    return "Unknown";
}

// A GPU (physical device) reported by an Instance. Pass `index` to Instance::create_device().
struct AdapterInfo {
    uint32_t index = 0;
    std::string name;
    std::string driver_name;
    std::string driver_info;
    AdapterType type = AdapterType::Unknown;
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint64_t device_local_memory_bytes = 0;
    // Graphics API version supported by the adapter (the Vulkan version for the Vulkan backend).
    core::Version api_version;
};

} // namespace byte::render::rhi
