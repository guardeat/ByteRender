#pragma once

#include <byte/core/log.hpp>

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace byte::tests {

// Records every log message; used to assert that code under test logs no warnings or errors.
class CaptureLogSink final : public core::LogSink {
public:
    struct Entry {
        core::LogLevel level;
        std::string message;
    };

    void write(core::LogLevel level, std::string_view message, const std::source_location&) override {
        const std::scoped_lock lock{m_mutex};
        m_entries.push_back(Entry{level, std::string{message}});
    }

    [[nodiscard]] std::vector<Entry> entries() const {
        const std::scoped_lock lock{m_mutex};
        return m_entries;
    }

    // Warnings and errors, formatted one per line (empty when clean).
    [[nodiscard]] std::string problems() const {
        const std::scoped_lock lock{m_mutex};
        std::string text;
        for (const Entry& entry : m_entries) {
            if (entry.level >= core::LogLevel::Warning) {
                text += std::string{core::to_string(entry.level)} + ": " + entry.message + "\n";
            }
        }
        return text;
    }

private:
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

} // namespace byte::tests
