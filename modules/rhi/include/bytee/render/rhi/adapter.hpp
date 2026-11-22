#pragma once

#include <bytee/core/version.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace bytee::render::rhi {

enum class AdapterType : uint8_t {
    Unknown,
    Discrete,
    Integrated,
    Virtual,
    Cpu,
};

[[nodiscard]] constexpr std::string_view to_string(AdapterType type) noexcept {
    switch (type) {
    case AdapterType::Unknown: return "Unknown";
    case AdapterType::Discrete: return "Discrete";
    case AdapterType::Integrated: return "Integrated";
    case AdapterType::Virtual: return "Virtual";
    case AdapterType::Cpu: return "Cpu";
    }
    return "Unknown";
}

// A GPU (physical device) reported by an Instance.
struct AdapterInfo {
    uint32_t index = 0; // position in Instance::adapters()
    std::string name;
    std::string driver_name;
    std::string driver_info;
    AdapterType type = AdapterType::Unknown;
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint64_t device_local_memory_bytes = 0;
    core::Version api_version; // graphics API version supported by the adapter (Vulkan version for Vulkan)
    bool is_supported = false; // meets every ByteRender requirement; if not, unsupported_reason says why
    std::string unsupported_reason;
};

} // namespace bytee::render::rhi
