#include <byte/core/log.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace byte::core;

namespace {

class CaptureSink final : public LogSink {
public:
    struct Entry {
        LogLevel level;
        std::string message;
        uint_least32_t line;
    };

    void write(LogLevel level, std::string_view message, const std::source_location& location) override {
        entries.push_back(Entry{level, std::string{message}, location.line()});
    }

    std::vector<Entry> entries;
};

} // namespace

TEST_CASE("Logger formats messages and filters by level") {
    CaptureSink sink;
    Logger logger{sink, LogLevel::Info};

    logger.debug("hidden {}", 1);
    logger.info("visible {} {}", 2, "args");
    logger.error("failed: {}", std::string{"reason"});

    REQUIRE(sink.entries.size() == 2);
    CHECK(sink.entries[0].level == LogLevel::Info);
    CHECK(sink.entries[0].message == "visible 2 args");
    CHECK(sink.entries[1].level == LogLevel::Error);
    CHECK(sink.entries[1].message == "failed: reason");
}

TEST_CASE("Logger captures the caller's source location") {
    CaptureSink sink;
    Logger logger{sink, LogLevel::Trace};

    const auto expected_line = std::source_location::current().line() + 1;
    logger.warning("here");

    REQUIRE(sink.entries.size() == 1);
    CHECK(sink.entries[0].line == expected_line);
}

TEST_CASE("Logger level can change at runtime") {
    CaptureSink sink;
    Logger logger{sink, LogLevel::Error};

    logger.info("dropped");
    logger.set_min_level(LogLevel::Off);
    logger.error("dropped too");
    logger.set_min_level(LogLevel::Trace);
    logger.trace("kept");

    REQUIRE(sink.entries.size() == 1);
    CHECK(sink.entries[0].message == "kept");
}
