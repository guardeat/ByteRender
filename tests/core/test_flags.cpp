#include <byte/core/flags.hpp>

#include <doctest/doctest.h>

#include <cstdint>

namespace test_flags {

enum class Usage : uint32_t {
    Sampled = 1u << 0,
    Storage = 1u << 1,
    ColorAttachment = 1u << 2,
};
BYTE_RENDER_ENABLE_FLAGS(Usage)

enum class NotFlags : uint32_t { A = 1 };

} // namespace test_flags

using byte::core::Flags;
using test_flags::Usage;

static_assert(byte::core::FlagEnum<Usage>);
static_assert(!byte::core::FlagEnum<test_flags::NotFlags>);
static_assert((Usage::Sampled | Usage::Storage).bits() == 0b011u);

TEST_CASE("Flags combine and query bits") {
    Flags<Usage> usage = Usage::Sampled | Usage::ColorAttachment;

    CHECK(usage.has(Usage::Sampled));
    CHECK(usage.has(Usage::ColorAttachment));
    CHECK_FALSE(usage.has(Usage::Storage));
    CHECK(usage.has(Usage::Sampled | Usage::ColorAttachment));
    CHECK_FALSE(usage.has(Usage::Sampled | Usage::Storage));
    CHECK(usage.has_any(Usage::Sampled | Usage::Storage));

    usage |= Usage::Storage;
    CHECK(usage.bits() == 0b111u);

    usage &= Usage::Storage;
    CHECK(usage == Flags<Usage>{Usage::Storage});
}

TEST_CASE("Empty flags") {
    const Flags<Usage> usage;
    CHECK(usage.none());
    CHECK_FALSE(usage.any());
    CHECK_FALSE(static_cast<bool>(usage));
}
