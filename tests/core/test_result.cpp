#include <byte/core/result.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

using namespace byte::core;

namespace {

Result<int> parse_positive(int value) {
    if (value <= 0) {
        return make_error(ErrorCode::InvalidArgument, "expected a positive value, got {}", value);
    }
    return value;
}

Result<void> check(bool ok) {
    if (!ok) {
        return Error{ErrorCode::NotSupported};
    }
    return {};
}

Result<int> doubled(int value) {
    BYTE_RENDER_TRY(check(value != 13));
    auto parsed = parse_positive(value);
    BYTE_RENDER_TRY(parsed);
    return *parsed * 2;
}

} // namespace

// Performance contract: errors and small results stay small.
static_assert(sizeof(Error) == 16);
static_assert(sizeof(Result<uint64_t>) <= 24);
static_assert(sizeof(Result<void>) <= 24);

TEST_CASE("Result holds a value") {
    const Result<int> result = parse_positive(5);
    REQUIRE(result.has_value());
    CHECK(static_cast<bool>(result));
    CHECK(result.value() == 5);
    CHECK(*result == 5);
}

TEST_CASE("Result holds an error with a formatted message") {
    const Result<int> result = parse_positive(-3);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code() == ErrorCode::InvalidArgument);
    CHECK(result.error().message() == "expected a positive value, got -3");
    CHECK(result.value_or(42) == 42);
}

TEST_CASE("Errors without a message carry only a code") {
    const Error error{ErrorCode::Timeout};
    CHECK(error.code() == ErrorCode::Timeout);
    CHECK(error.message().empty());
    CHECK(error.location().line() == 0);
}

TEST_CASE("Error locations are recorded only when enabled") {
    const auto expected_line = std::source_location::current().line() + 1;
    const Error error = make_error(ErrorCode::OutOfMemory, "out of memory");
#if defined(BYTE_RENDER_ENABLE_ERROR_LOCATIONS)
    CHECK(error.location().line() == expected_line);
    // Paths are relative to the source tree (-fmacro-prefix-map), never absolute.
    CHECK(std::string_view{error.location().file_name()} == "tests/core/test_result.cpp");
#else
    static_cast<void>(expected_line);
    CHECK(error.location().line() == 0);
#endif
}

TEST_CASE("Errors are copyable and movable") {
    const Error original = make_error(ErrorCode::DeviceLost, "lost {}", "device");
    const Error copy = original; // NOLINT(performance-unnecessary-copy-initialization): testing the copy
    CHECK(copy.code() == ErrorCode::DeviceLost);
    CHECK(copy.message() == "lost device");
    CHECK(original.message() == "lost device");

    Error moved = Error{original};
    CHECK(moved.message() == "lost device");
    Error assigned;
    assigned = std::move(moved);
    CHECK(assigned.code() == ErrorCode::DeviceLost);
}

TEST_CASE("Result supports move-only and converting values") {
    Result<std::unique_ptr<int>> owned = std::make_unique<int>(7);
    REQUIRE(owned);
    const std::unique_ptr<int> value = std::move(owned).value();
    CHECK(*value == 7);

    const Result<std::string> text = "hello";
    REQUIRE(text);
    CHECK(text->size() == 5);
}

TEST_CASE("Result<void>") {
    CHECK(check(true).has_value());
    const Result<void> failed = check(false);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code() == ErrorCode::NotSupported);
}

TEST_CASE("BYTE_RENDER_TRY propagates errors and continues on success") {
    CHECK(doubled(4).value() == 8);
    CHECK(doubled(13).error().code() == ErrorCode::NotSupported);
    CHECK(doubled(-1).error().code() == ErrorCode::InvalidArgument);
}

TEST_CASE("ErrorCode names") {
    CHECK(to_string(ErrorCode::OutOfMemory) == "OutOfMemory");
    CHECK(to_string(ErrorCode::Timeout) == "Timeout");
}
