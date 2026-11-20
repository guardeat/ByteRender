#pragma once

#include <byte/core/export.hpp>

#include <atomic>
#include <concepts>
#include <cstdint>
#include <format>
#include <mutex>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

namespace byte::core {

enum class LogLevel : uint8_t {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Off,
};

constexpr std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error: return "error";
    case LogLevel::Off: return "off";
    }
    return "unknown";
}

// Destination for log messages. Implementations must be thread-safe.
class BYTE_RENDER_CORE_API LogSink {
public:
    virtual ~LogSink();
    virtual void write(LogLevel level, std::string_view message, const std::source_location& location) = 0;

protected:
    LogSink() = default;
    LogSink(const LogSink&) = default;
    LogSink& operator=(const LogSink&) = default;
};

// Writes to stdout (trace..info) and stderr (warning, error).
class BYTE_RENDER_CORE_API ConsoleLogSink final : public LogSink {
public:
    void write(LogLevel level, std::string_view message, const std::source_location& location) override;

private:
    std::mutex m_mutex;
};

// A format string that also captures the caller's source location.
template <class... Args>
struct LogFormat {
    template <class S>
        requires std::convertible_to<const S&, std::string_view>
    consteval LogFormat(const S& text, std::source_location caller = std::source_location::current())
        : format(text)
        , location(caller) {}

    std::format_string<Args...> format;
    std::source_location location;
};

// Formats messages and forwards them to a sink. Not a global: owned by the application and passed
// to engine objects that need it. The sink must outlive the logger.
//
//   ConsoleLogSink sink;
//   Logger logger{sink, LogLevel::Debug};
//   logger.info("created {} textures", count);
class Logger {
public:
    explicit Logger(LogSink& sink, LogLevel min_level = LogLevel::Info) noexcept
        : m_sink(&sink)
        , m_min_level(min_level) {}

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_min_level(LogLevel level) noexcept { m_min_level.store(level, std::memory_order_relaxed); }
    [[nodiscard]] LogLevel min_level() const noexcept { return m_min_level.load(std::memory_order_relaxed); }
    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept {
        return level != LogLevel::Off && level >= min_level();
    }

    template <class... Args>
    void trace(LogFormat<std::type_identity_t<Args>...> format, Args&&... args) {
        log(LogLevel::Trace, format, std::forward<Args>(args)...);
    }
    template <class... Args>
    void debug(LogFormat<std::type_identity_t<Args>...> format, Args&&... args) {
        log(LogLevel::Debug, format, std::forward<Args>(args)...);
    }
    template <class... Args>
    void info(LogFormat<std::type_identity_t<Args>...> format, Args&&... args) {
        log(LogLevel::Info, format, std::forward<Args>(args)...);
    }
    template <class... Args>
    void warning(LogFormat<std::type_identity_t<Args>...> format, Args&&... args) {
        log(LogLevel::Warning, format, std::forward<Args>(args)...);
    }
    template <class... Args>
    void error(LogFormat<std::type_identity_t<Args>...> format, Args&&... args) {
        log(LogLevel::Error, format, std::forward<Args>(args)...);
    }

    // Unformatted message with an explicit level, e.g. for forwarding messages from a validation layer.
    void write(LogLevel level, std::string_view message,
               const std::source_location& location = std::source_location::current()) {
        if (is_enabled(level)) {
            m_sink->write(level, message, location);
        }
    }

private:
    template <class... Args>
    void log(LogLevel level, const LogFormat<Args...>& format, Args&&... args) {
        if (is_enabled(level)) {
            m_sink->write(level, std::format(format.format, std::forward<Args>(args)...), format.location);
        }
    }

    LogSink* m_sink;
    std::atomic<LogLevel> m_min_level;
};

} // namespace byte::core
