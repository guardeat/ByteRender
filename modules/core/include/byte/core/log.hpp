#pragma once

#include <byte/core/export.hpp>
#include <byte/core/located_format.hpp>

#include <atomic>
#include <cstdint>
#include <format>
#include <mutex>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

#if !defined(BYTE_RENDER_LOG_LEVEL)
    #error "BYTE_RENDER_LOG_LEVEL must be defined (set by the byte::core CMake target)"
#endif

namespace byte::core {

// Numeric values match BYTE_RENDER_LOG_LEVEL (see byte_render_options.cmake).
enum class LogLevel : uint8_t {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Off = 5,
};

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) noexcept {
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

// Lowest level compiled into this build. Logging calls below it compile to nothing (their argument expressions are
// still evaluated, so keep expensive computations out of trace/debug calls or guard them with is_log_level_compiled).
inline constexpr LogLevel k_compiled_log_level = static_cast<LogLevel>(BYTE_RENDER_LOG_LEVEL);

[[nodiscard]] constexpr bool is_log_level_compiled(LogLevel level) noexcept {
    return level != LogLevel::Off && level >= k_compiled_log_level;
}

// Destination for log messages (console, file, editor panel, ...). Implementations must be thread-safe.
class BYTE_RENDER_CORE_API LogSink {
public:
    virtual ~LogSink();
    virtual void write(LogLevel level, std::string_view message, const std::source_location& location) = 0;

protected:
    LogSink() = default;
    LogSink(const LogSink&) = default;
    LogSink& operator=(const LogSink&) = default;
};

// stdout for trace..info, stderr for warning and error (with the source location).
class BYTE_RENDER_CORE_API ConsoleLogSink final : public LogSink {
private:
    std::mutex mutex_;

public:
    void write(LogLevel level, std::string_view message, const std::source_location& location) override;
};

// Formats messages and forwards them to a sink. Owned by the application and injected into engine objects
// (not a global). The sink must outlive the logger.
//
//   ConsoleLogSink sink;
//   Logger logger{sink, LogLevel::Debug};
//   logger.info("loaded {} textures", count);
class Logger {
private:
    LogSink* sink_;
    std::atomic<LogLevel> min_level_;

public:
    explicit Logger(LogSink& sink, LogLevel min_level = LogLevel::Info) noexcept
        : sink_(&sink)
        , min_level_(min_level) {}

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Runtime filter on top of the compile-time level.
    void set_min_level(LogLevel level) noexcept { min_level_.store(level, std::memory_order_relaxed); }
    [[nodiscard]] LogLevel min_level() const noexcept { return min_level_.load(std::memory_order_relaxed); }
    [[nodiscard]] bool is_enabled(LogLevel level) const noexcept {
        return is_log_level_compiled(level) && level >= min_level();
    }

    template <class... Args_>
    void trace(LocatedFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
        log<LogLevel::Trace>(format, std::forward<Args_>(args)...);
    }
    template <class... Args_>
    void debug(LocatedFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
        log<LogLevel::Debug>(format, std::forward<Args_>(args)...);
    }
    template <class... Args_>
    void info(LocatedFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
        log<LogLevel::Info>(format, std::forward<Args_>(args)...);
    }
    template <class... Args_>
    void warning(LocatedFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
        log<LogLevel::Warning>(format, std::forward<Args_>(args)...);
    }
    template <class... Args_>
    void error(LocatedFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
        log<LogLevel::Error>(format, std::forward<Args_>(args)...);
    }

    // Pre-formatted message with a runtime level, e.g. forwarded from a graphics-API validation layer.
    void write(LogLevel level, std::string_view message,
               const std::source_location& location = std::source_location::current()) {
        if (is_enabled(level)) {
            sink_->write(level, message, location);
        }
    }

private:
    template <LogLevel Level_, class... Args_>
    void log(const LocatedFormat<Args_...>& format, Args_&&... args) {
        if constexpr (is_log_level_compiled(Level_)) {
            if (Level_ >= min_level()) {
                sink_->write(Level_, std::format(format.format, std::forward<Args_>(args)...), format.location);
            }
        }
    }
};

} // namespace byte::core
