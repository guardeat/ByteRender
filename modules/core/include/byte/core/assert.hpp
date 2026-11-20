#pragma once

#include <byte/core/export.hpp>

#include <format>
#include <source_location>
#include <string_view>
#include <utility>

namespace byte::core::detail {

// Prints the failure to stderr and aborts. Never returns.
[[noreturn]] BYTE_RENDER_CORE_API void assert_failed(std::string_view expression, std::string_view message,
                                                     const std::source_location& location);

template <class... Args>
[[noreturn]] void assert_failed(std::string_view expression, const std::source_location& location,
                                std::format_string<Args...> format, Args&&... args) {
    assert_failed(expression, std::format(format, std::forward<Args>(args)...), location);
}

} // namespace byte::core::detail

// BYTE_RENDER_ASSERT(condition, "message")
// BYTE_RENDER_ASSERT(condition, "index {} out of range [0, {})", index, size)
//
// For programmer errors only. Enabled in Debug (BYTE_RENDER_ASSERTS=AUTO) or with BYTE_RENDER_ASSERTS=ON.
// When disabled the condition is not evaluated.
#if defined(BYTE_RENDER_ENABLE_ASSERTS)
    #define BYTE_RENDER_ASSERT(condition, ...)                                                             \
        do {                                                                                               \
            if (!(condition)) [[unlikely]] {                                                               \
                ::byte::core::detail::assert_failed(#condition, std::source_location::current(),           \
                                                    __VA_ARGS__);                                          \
            }                                                                                              \
        } while (false)
#else
    #define BYTE_RENDER_ASSERT(condition, ...)                                                             \
        do {                                                                                               \
            (void)sizeof(condition);                                                                       \
        } while (false)
#endif
