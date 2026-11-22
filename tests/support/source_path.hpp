#pragma once

// Source paths as Bytee reports them (relative to the source tree), with '/' separators on every platform, so tests
// compare them the same way everywhere (MSVC reports Windows separators).

#include <algorithm>
#include <string>
#include <string_view>

namespace bytee::tests {

[[nodiscard]] inline std::string generic_path(std::string_view path) {
    std::string generic{path};
    std::ranges::replace(generic, '\\', '/');
    return generic;
}

} // namespace bytee::tests
