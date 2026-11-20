#include <byte/core/result.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

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
        return Error{.code = ErrorCode::NotSupported, .message = "not supported"};
    }
    return {};
}

Result<std::unique_ptr<int>> make_owned(int value) {
    auto owned = std::make_unique<int>(value);
    return owned;
}

} // namespace

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
    CHECK(result.error().code == ErrorCode::InvalidArgument);
    CHECK(result.error().message == "expected a positive value, got -3");
    CHECK(result.value_or(42) == 42);
}

TEST_CASE("Result supports move-only types") {
    Result<std::unique_ptr<int>> result = make_owned(7);
    REQUIRE(result);
    std::unique_ptr<int> owned = std::move(result).value();
    REQUIRE(owned != nullptr);
    CHECK(*owned == 7);
}

TEST_CASE("Result converts from compatible types") {
    const Result<std::string> result = "hello";
    REQUIRE(result);
    CHECK(result->size() == 5);
}

TEST_CASE("Result<void>") {
    CHECK(check(true).has_value());

    const Result<void> failed = check(false);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().code == ErrorCode::NotSupported);
}

TEST_CASE("ErrorCode to_string") {
    CHECK(to_string(ErrorCode::OutOfMemory) == "OutOfMemory");
}
