#include <byte/core/log.hpp>

#include <cstdio>
#include <string>

namespace byte::core {

LogSink::~LogSink() = default;

void ConsoleLogSink::write(LogLevel level, std::string_view message, const std::source_location& location) {
    const bool is_problem = level >= LogLevel::Warning;
    const std::string line = is_problem ? std::format("[byte][{}] {} ({}:{})\n", to_string(level), message,
                                                      location.file_name(), location.line())
                                        : std::format("[byte][{}] {}\n", to_string(level), message);

    const std::scoped_lock lock{m_mutex};
    std::FILE* stream = is_problem ? stderr : stdout;
    std::fputs(line.c_str(), stream);
    if (is_problem) {
        std::fflush(stream);
    }
}

} // namespace byte::core
