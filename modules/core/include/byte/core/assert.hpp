#pragma once

#include <byte/core/export.hpp>

#include <format>
#include <source_location>
#include <string_view>
#include <utility>

namespace byte::core::detail {

// Prints the failed condition, message and location to stderr, then aborts.
[[noreturn]] BYTE_RENDER_CORE_API void assert_failed(std::string_view expression, std::string_view message,
                                                     const std::source_location& location) noexcept;

template <class... Args_>
[[noreturn]] void assert_failed(std::string_view expression, const std::source_location& location,
                                std::format_string<Args_...> format, Args_&&... args) noexcept {
    assert_failed(expression, std::format(format, std::forward<Args_>(args)...), location);
}

// Never defined: only named inside sizeof() so disabled asserts still "use" their operands (no unused warnings)
// without evaluating them.
template <class... Args_>
bool assert_operands(Args_&&... args) noexcept;

} // namespace byte::core::detail

// BYTE_RENDER_ASSERT(condition, "message")
// BYTE_RENDER_ASSERT(condition, "index {} out of range [0, {})", index, size)
//
// For programmer errors only (runtime failures return core::Result). Enabled in Debug (BYTE_RENDER_ASSERTS=AUTO)
// or with BYTE_RENDER_ASSERTS=ON. When disabled, neither the condition nor the message arguments are evaluated.
#if defined(BYTE_RENDER_ENABLE_ASSERTS)
    #define BYTE_RENDER_ASSERT(condition, ...)                                                                         \
        do {                                                                                                           \
            if (!(condition)) [[unlikely]] {                                                                           \
                ::byte::core::detail::assert_failed(#condition, std::source_location::current(), __VA_ARGS__);         \
            }                                                                                                          \
        } while (false)
#else
    #define BYTE_RENDER_ASSERT(condition, ...)                                                                         \
        do {                                                                                                           \
            static_cast<void>(sizeof(::byte::core::detail::assert_operands((condition), __VA_ARGS__)));                \
        } while (false)
#endif
