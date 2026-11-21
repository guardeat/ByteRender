#include <byte/core/assert.hpp>

#include <doctest/doctest.h>

// Behaviour shared by both configurations: a passing assert does nothing and never aborts.
TEST_CASE("A passing assert has no effect") {
    int evaluations = 0;
    const auto passes = [&] {
        ++evaluations;
        return true;
    };
    BYTE_RENDER_ASSERT(passes(), "never shown {}", 42);

#if defined(BYTE_RENDER_ENABLE_ASSERTS)
    CHECK(evaluations == 1); // evaluated exactly once
#else
    CHECK(evaluations == 0); // compiled out
#endif
}
