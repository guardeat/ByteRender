// Compiled with asserts forced off, whatever the build configuration, to test the disabled form.
#undef BYTEE_ENABLE_ASSERTS
#include <bytee/core/assert.hpp>

#include <doctest/doctest.h>

TEST_CASE("Disabled asserts evaluate neither the condition nor the message arguments") {
    int evaluations = 0;
    const auto count = [&] {
        ++evaluations;
        return false;
    };
    // Only used inside the assert: must not trigger unused-variable warnings when asserts are off.
    const int only_in_message = 7;
    BYTEE_ASSERT(count(), "value {} {}", only_in_message, count());
    CHECK(evaluations == 0);
}
