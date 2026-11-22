#include <bytee/core/assert.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#if defined(_MSC_VER)
    #include <stdlib.h> // _set_abort_behavior
#endif

namespace bytee::core::detail {

void assert_failed(std::string_view expression, std::string_view message,
                   const std::source_location& location) noexcept {
    const std::string text =
        std::format("[bytee][assert] {}:{}: in '{}'\n  condition: {}\n  message:   {}\n", location.file_name(),
                    location.line(), location.function_name(), expression, message);
    std::fputs(text.c_str(), stderr);
    std::fflush(stderr);
#if defined(_MSC_VER)
    // The report above says everything: no CRT message box or error-reporting dialog (they block unattended runs).
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    std::abort();
}

} // namespace bytee::core::detail
