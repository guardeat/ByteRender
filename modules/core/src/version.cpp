#include <byte/core/version.hpp>

namespace byte::core {

Version version() {
    return Version{
        .major = BYTE_RENDERER_VERSION_MAJOR,
        .minor = BYTE_RENDERER_VERSION_MINOR,
        .patch = BYTE_RENDERER_VERSION_PATCH,
    };
}

const char* version_string() {
    return BYTE_RENDERER_VERSION_STRING;
}

} // namespace byte::core
