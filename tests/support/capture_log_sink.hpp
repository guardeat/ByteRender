#pragma once

#include <byte/core/log.hpp>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace byte::tests {

// Records every log message. GPU tests use problems() to enforce the zero-noise policy.
class CaptureLogSink final : public core::LogSink {
public:
    struct Entry {
        core::LogLevel level;
        std::string message;
        std::string file;
        uint_least32_t line;
    };

private:
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;

public:
    void write(core::LogLevel level, std::string_view message, const std::source_location& location) override {
        const std::scoped_lock lock{mutex_};
        entries_.push_back(Entry{level, std::string{message}, location.file_name(), location.line()});
    }

    [[nodiscard]] std::vector<Entry> entries() const {
        const std::scoped_lock lock{mutex_};
        return entries_;
    }

    // Warnings and errors, one per line; empty when clean.
    [[nodiscard]] std::string problems() const {
        const std::scoped_lock lock{mutex_};
        std::string text;
        for (const Entry& entry : entries_) {
            if (entry.level >= core::LogLevel::Warning) {
                text += std::string{core::to_string(entry.level)} + ": " + entry.message + "\n";
            }
        }
        return text;
    }
};

} // namespace byte::tests
