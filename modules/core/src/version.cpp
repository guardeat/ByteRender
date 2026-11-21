#include <bytee/core/version.hpp>

namespace bytee::core {

Version version() noexcept {
    return Version{
        .major = BYTEE_VERSION_MAJOR,
        .minor = BYTEE_VERSION_MINOR,
        .patch = BYTEE_VERSION_PATCH,
    };
}

std::string_view version_string() noexcept {
    return BYTEE_VERSION_STRING;
}

} // namespace bytee::core
