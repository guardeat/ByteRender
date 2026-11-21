#include <bytee/core/assert.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace bytee::core::detail {

void assert_failed(std::string_view expression, std::string_view message,
                   const std::source_location& location) noexcept {
    const std::string text =
        std::format("[bytee][assert] {}:{}: in '{}'\n  condition: {}\n  message:   {}\n", location.file_name(),
                    location.line(), location.function_name(), expression, message);
    std::fputs(text.c_str(), stderr);
    std::fflush(stderr);
    std::abort();
}

} // namespace bytee::core::detail
