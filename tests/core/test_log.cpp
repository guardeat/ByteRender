#include "support/capture_log_sink.hpp"

#include <byte/core/log.hpp>

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using namespace byte::core;
using byte::tests::CaptureLogSink;

TEST_CASE("Logger formats messages and filters by runtime level") {
    CaptureLogSink sink;
    Logger logger{sink, LogLevel::Warning};

    logger.info("hidden {}", 1);
    logger.warning("visible {} {}", 2, "args");
    logger.error("failed: {}", std::string{"reason"});

    const auto entries = sink.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].level == LogLevel::Warning);
    CHECK(entries[0].message == "visible 2 args");
    CHECK(entries[1].level == LogLevel::Error);
    CHECK(entries[1].message == "failed: reason");
}

TEST_CASE("Logger captures the caller's source location as a relative path") {
    CaptureLogSink sink;
    Logger logger{sink, LogLevel::Trace};

    const auto expected_line = std::source_location::current().line() + 1;
    logger.error("here");

    const auto entries = sink.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].line == expected_line);
    CHECK(entries[0].file == "tests/core/test_log.cpp");
}

TEST_CASE("Calls below the compile-time level compile to nothing") {
    CaptureLogSink sink;
    Logger logger{sink, LogLevel::Trace}; // runtime allows everything

    logger.trace("trace-only-marker");
    logger.debug("debug-only-marker");
    logger.info("info-marker");

    std::size_t expected = 1; // info is compiled in every configuration
    expected += is_log_level_compiled(LogLevel::Trace) ? 1 : 0;
    expected += is_log_level_compiled(LogLevel::Debug) ? 1 : 0;
    CHECK(sink.entries().size() == expected);

    CHECK(is_log_level_compiled(LogLevel::Error));
    CHECK_FALSE(is_log_level_compiled(LogLevel::Off));
}

TEST_CASE("Runtime level changes and pre-formatted writes") {
    CaptureLogSink sink;
    Logger logger{sink, LogLevel::Error};

    logger.warning("dropped");
    logger.set_min_level(LogLevel::Off);
    logger.error("dropped too");
    logger.set_min_level(LogLevel::Warning);
    logger.write(LogLevel::Warning, "forwarded {braces} stay literal");

    const auto entries = sink.entries();
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].message == "forwarded {braces} stay literal");
    CHECK(logger.min_level() == LogLevel::Warning);
}
