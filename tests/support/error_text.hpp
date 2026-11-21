#pragma once

#include <bytee/core/result.hpp>

#include <string_view>

namespace bytee::tests {

// Message of a failed Result, or empty for a successful one. Use it in doctest *_MESSAGE macros instead of
// `result.error().message()`: doctest may evaluate the message even when the check passes (e.g. with -s), and
// calling error() on a successful Result is a programmer error that asserts.
template <class Value_>
[[nodiscard]] std::string_view error_text(const core::Result<Value_>& result) noexcept {
    return result.has_value() ? std::string_view{} : result.error().message();
}

} // namespace bytee::tests
