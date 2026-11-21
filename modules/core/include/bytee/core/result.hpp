#pragma once

#include <bytee/core/assert.hpp>
#include <bytee/core/located_format.hpp>

#include <concepts>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace bytee::core {

enum class ErrorCode : uint32_t {
    Unknown,
    InvalidArgument,
    OutOfMemory,
    NotSupported,
    InitializationFailed,
    DeviceLost,
    Timeout,
};

[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::Unknown: return "Unknown";
    case ErrorCode::InvalidArgument: return "InvalidArgument";
    case ErrorCode::OutOfMemory: return "OutOfMemory";
    case ErrorCode::NotSupported: return "NotSupported";
    case ErrorCode::InitializationFailed: return "InitializationFailed";
    case ErrorCode::DeviceLost: return "DeviceLost";
    case ErrorCode::Timeout: return "Timeout";
    }
    return "Unknown";
}

// A runtime failure: a code plus optional details. 16 bytes; an error without a message never allocates
// (`Error{ErrorCode::Timeout}`), so hot polling paths can fail cheaply. Messages are allocated only on failure.
// The source location is recorded only when BYTEE_ENABLE_ERROR_LOCATIONS is defined (Debug by default).
class Error {
private:
    struct Details {
        std::string message;
        std::source_location location;
    };

    ErrorCode code_ = ErrorCode::Unknown;
    std::unique_ptr<Details> details_; // null: no message, no location

public:
    Error() noexcept = default;
    explicit Error(ErrorCode code) noexcept
        : code_(code) {}
    Error(ErrorCode code, std::string message, std::source_location location = {})
        : code_(code)
        , details_(std::make_unique<Details>(Details{.message = std::move(message), .location = location})) {}

    Error(const Error& other)
        : code_(other.code_)
        , details_(other.details_ ? std::make_unique<Details>(*other.details_) : nullptr) {}
    Error& operator=(const Error& other) {
        if (this != &other) {
            code_ = other.code_;
            details_ = other.details_ ? std::make_unique<Details>(*other.details_) : nullptr;
        }
        return *this;
    }
    Error(Error&&) noexcept = default;
    Error& operator=(Error&&) noexcept = default;
    ~Error() = default;

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }
    [[nodiscard]] std::string_view message() const noexcept {
        return details_ ? std::string_view{details_->message} : std::string_view{};
    }
    // Where the error was created; default (line 0) when locations are not recorded.
    [[nodiscard]] std::source_location location() const noexcept {
        return details_ ? details_->location : std::source_location{};
    }
};

#if defined(BYTEE_ENABLE_ERROR_LOCATIONS)
template <class... Args_>
using ErrorFormat = LocatedFormat<Args_...>;
#else
template <class... Args_>
using ErrorFormat = std::format_string<Args_...>;
#endif

// Creates an error with a formatted message: `return make_error(ErrorCode::OutOfMemory, "texture '{}'", name);`
template <class... Args_>
[[nodiscard]] Error make_error(ErrorCode code, ErrorFormat<std::type_identity_t<Args_>...> format, Args_&&... args) {
#if defined(BYTEE_ENABLE_ERROR_LOCATIONS)
    return Error{code, std::format(format.format, std::forward<Args_>(args)...), format.location};
#else
    return Error{code, std::format(format, std::forward<Args_>(args)...)};
#endif
}

// Value-or-error return type for fallible operations (expected-like; C++20 has no std::expected).
//
//   Result<Texture> Device::create_texture(const TextureDesc& desc) {
//       if (failed) return make_error(ErrorCode::OutOfMemory, "texture '{}'", desc.debug_name);
//       return texture;
//   }
//
// Accessing value() on an error, or error() on a value, is a programmer error and asserts.
template <class Value_>
class [[nodiscard]] Result {
    static_assert(!std::is_reference_v<Value_>, "Result<T&> is not supported");
    static_assert(!std::is_same_v<std::remove_cv_t<Value_>, Error>, "Result<Error> is ambiguous");

private:
    std::variant<Value_, Error> storage_;

public:
    template <class Other_ = Value_>
        requires(std::constructible_from<Value_, Other_&&> && !std::same_as<std::remove_cvref_t<Other_>, Result> &&
                 !std::same_as<std::remove_cvref_t<Other_>, Error>)
    constexpr explicit(!std::convertible_to<Other_&&, Value_>) Result(Other_&& value)
        : storage_(std::in_place_index<0>, std::forward<Other_>(value)) {}

    Result(Error error) noexcept
        : storage_(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return storage_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr Value_& value() & {
        BYTEE_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] constexpr const Value_& value() const& {
        BYTEE_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] constexpr Value_&& value() && {
        BYTEE_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return std::move(*std::get_if<0>(&storage_));
    }

    [[nodiscard]] constexpr const Error& error() const& {
        BYTEE_ASSERT(!has_value(), "Result::error() called on a value");
        return *std::get_if<1>(&storage_);
    }
    [[nodiscard]] constexpr Error&& error() && {
        BYTEE_ASSERT(!has_value(), "Result::error() called on a value");
        return std::move(*std::get_if<1>(&storage_));
    }

    template <class Fallback_>
    [[nodiscard]] constexpr Value_ value_or(Fallback_&& fallback) const& {
        return has_value() ? *std::get_if<0>(&storage_) : static_cast<Value_>(std::forward<Fallback_>(fallback));
    }
    template <class Fallback_>
    [[nodiscard]] constexpr Value_ value_or(Fallback_&& fallback) && {
        return has_value() ? std::move(*std::get_if<0>(&storage_))
                           : static_cast<Value_>(std::forward<Fallback_>(fallback));
    }

    constexpr Value_& operator*() & { return value(); }
    constexpr const Value_& operator*() const& { return value(); }
    constexpr Value_&& operator*() && { return std::move(*this).value(); }
    constexpr Value_* operator->() { return &value(); }
    constexpr const Value_* operator->() const { return &value(); }

private:
    constexpr std::string_view error_message() const noexcept {
        const Error* error = std::get_if<1>(&storage_);
        return error ? error->message() : std::string_view{};
    }
};

// Result of an operation without a value: `return {};` on success, `return Error{...};` on failure.
template <>
class [[nodiscard]] Result<void> {
private:
    std::optional<Error> error_;

public:
    constexpr Result() noexcept = default;
    Result(Error error) noexcept
        : error_(std::move(error)) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return !error_.has_value(); }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr void value() const {
        BYTEE_ASSERT(has_value(), "Result<void>::value() called on an error: {}", error_->message());
    }

    [[nodiscard]] constexpr const Error& error() const& {
        BYTEE_ASSERT(!has_value(), "Result::error() called on a value");
        return *error_;
    }
    [[nodiscard]] constexpr Error&& error() && {
        BYTEE_ASSERT(!has_value(), "Result::error() called on a value");
        return std::move(*error_);
    }
};

} // namespace bytee::core

// Returns the error of a failed Result from the enclosing function (whose return type must be a Result);
// the value of a successful Result is discarded.
//   BYTEE_TRY(queue.submit(...));
#define BYTEE_TRY(expression)                                                                                          \
    do {                                                                                                               \
        if (auto bytee_try_result_ = (expression); !bytee_try_result_) {                                               \
            return std::move(bytee_try_result_).error();                                                               \
        }                                                                                                              \
    } while (false)
