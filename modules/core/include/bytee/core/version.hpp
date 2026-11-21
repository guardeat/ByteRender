#pragma once

#include <bytee/core/export.hpp>

#include <cstdint>
#include <string_view>

namespace bytee::core {

struct Version {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;

    friend constexpr bool operator==(const Version&, const Version&) noexcept = default;
};

// Version of the Bytee library that was linked (may differ from the headers in shared builds).
[[nodiscard]] BYTEE_CORE_API Version version() noexcept;
[[nodiscard]] BYTEE_CORE_API std::string_view version_string() noexcept;

} // namespace bytee::core
