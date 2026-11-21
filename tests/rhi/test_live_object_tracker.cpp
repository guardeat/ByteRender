#include "common/live_object_tracker.hpp"
#include "support/capture_log_sink.hpp"

#include <doctest/doctest.h>

using bytee::render::rhi::detail::LiveObjectTracker;

TEST_CASE("LiveObjectTracker counts objects and reports leaks in creation order") {
    bytee::tests::CaptureLogSink sink;
    bytee::core::Logger logger{sink, bytee::core::LogLevel::Trace};

    LiveObjectTracker tracker;
    const auto albedo = tracker.add("Texture", "albedo");
    const auto vertices = tracker.add("Buffer", "vertices");
    const auto normal = tracker.add("Texture", "normal");
    CHECK(tracker.count() == 3);

    tracker.remove(vertices);
    CHECK(tracker.report_leaks(logger, "Device 'main'") == 2);

    const auto entries = sink.entries();
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].level == bytee::core::LogLevel::Error);
    CHECK(entries[0].message == "Device 'main' destroyed while Texture 'albedo' is still alive");
    CHECK(entries[1].message == "Device 'main' destroyed while Texture 'normal' is still alive");

    tracker.remove(albedo);
    tracker.remove(normal);
    CHECK(tracker.count() == 0);
    CHECK(tracker.report_leaks(logger, "Device 'main'") == 0);
}
