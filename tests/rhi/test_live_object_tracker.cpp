#include "common/live_object_tracker.hpp"
#include "support/capture_log_sink.hpp"

#include <doctest/doctest.h>

using byte::render::rhi::detail::LiveObjectTracker;

TEST_CASE("LiveObjectTracker counts objects and reports leaks in creation order") {
    byte::tests::CaptureLogSink sink;
    byte::core::Logger logger{sink, byte::core::LogLevel::Trace};

    LiveObjectTracker tracker;
    const auto albedo = tracker.add("Texture", "albedo");
    const auto vertices = tracker.add("Buffer", "vertices");
    const auto normal = tracker.add("Texture", "normal");
    CHECK(tracker.count() == 3);

    tracker.remove(vertices);
    CHECK(tracker.count() == 2);

    CHECK(tracker.report_leaks(logger, "Device") == 2);
    const auto entries = sink.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].level == byte::core::LogLevel::Error);
    CHECK(entries[0].message == "Device destroyed while Texture 'albedo' is still alive");
    CHECK(entries[1].message == "Device destroyed while Texture 'normal' is still alive");

    tracker.remove(albedo);
    tracker.remove(normal);
    CHECK(tracker.count() == 0);
    CHECK(tracker.report_leaks(logger, "Device") == 0);
}
