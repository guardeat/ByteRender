#pragma once

#include <byte/core/Export.h>

#include <cstdint>

namespace byte::core {

struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
};

// Version of the ByteRender library that was linked.
BYTE_RENDERER_CORE_API Version version();
BYTE_RENDERER_CORE_API const char* version_string();

} // namespace byte::core
