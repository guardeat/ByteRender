#include <bytee/core/log.hpp>

#include <cstdio>
#include <string>

namespace bytee::core {

LogSink::~LogSink() = default;

void ConsoleLogSink::write(LogLevel level, std::string_view message, const std::source_location& location) {
    const bool is_problem = level >= LogLevel::Warning;
    const std::string line =
        is_problem && location.line() != 0
            ? std::format("[bytee][{}] {} ({}:{})\n", to_string(level), message, location.file_name(), location.line())
            : std::format("[bytee][{}] {}\n", to_string(level), message);

    std::FILE* stream = is_problem ? stderr : stdout;
    const std::scoped_lock lock{mutex_};
    std::fputs(line.c_str(), stream);
    if (is_problem) {
        std::fflush(stream);
    }
}

} // namespace bytee::core
