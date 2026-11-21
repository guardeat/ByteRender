#pragma once

#include <concepts>
#include <format>
#include <source_location>
#include <string_view>

namespace byte::core {

// A compile-time checked format string that also captures the caller's source location.
// Used as a function parameter so call sites stay plain: `logger.info("loaded {} files", count);`
template <class... Args_>
struct LocatedFormat {
    std::format_string<Args_...> format;
    std::source_location location;

    template <class String_>
        requires std::convertible_to<const String_&, std::string_view>
    consteval LocatedFormat(const String_& text, std::source_location caller = std::source_location::current())
        : format(text)
        , location(caller) {}
};

} // namespace byte::core
