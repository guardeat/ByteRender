#include <byte/core/version.hpp>

namespace byte::core {

Version version() noexcept {
    return Version{
        .major = BYTE_RENDER_VERSION_MAJOR,
        .minor = BYTE_RENDER_VERSION_MINOR,
        .patch = BYTE_RENDER_VERSION_PATCH,
    };
}

std::string_view version_string() noexcept {
    return BYTE_RENDER_VERSION_STRING;
}

} // namespace byte::core
