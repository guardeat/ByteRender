#pragma once

#include <bytee/core/flags.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace bytee::render::rhi {

enum class BufferUsage : uint32_t {
    TransferSrc = 1u << 0,
    TransferDst = 1u << 1,
    Uniform = 1u << 2,
    Storage = 1u << 3,
    Index = 1u << 4,
    Vertex = 1u << 5,
    Indirect = 1u << 6,
};
BYTEE_ENABLE_FLAGS(BufferUsage)

// Where a buffer's memory lives and how the CPU may access it.
enum class MemoryLocation : uint8_t {
    Device,   // GPU-local, fastest for the GPU; no CPU access (fill it with uploads)
    Upload,   // CPU writes, GPU reads (staging, per-frame data); Device::write()
    Readback, // GPU writes, CPU reads (results, screenshots); Device::read()
};

[[nodiscard]] constexpr std::string_view to_string(MemoryLocation location) noexcept {
    switch (location) {
    case MemoryLocation::Device: return "Device";
    case MemoryLocation::Upload: return "Upload";
    case MemoryLocation::Readback: return "Readback";
    }
    return "Unknown";
}

struct BufferDesc {
    uint64_t size = 0;
    core::Flags<BufferUsage> usage;
    MemoryLocation memory = MemoryLocation::Device;
    std::string_view debug_name;
};

struct BufferInfo {
    uint64_t size = 0;
    core::Flags<BufferUsage> usage;
    MemoryLocation memory = MemoryLocation::Device;
    uint64_t device_address = 0; // GPU address for shaders (bindless access through buffer device address)
    std::string debug_name;
};

} // namespace bytee::render::rhi
