#pragma once

#include <byte/core/assert.hpp>

#include <concepts>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace byte::core {

enum class ErrorCode : uint32_t {
    Unknown,
    InvalidArgument,
    OutOfMemory,
    NotSupported,
    InitializationFailed,
    DeviceLost,
    Timeout,
};

constexpr std::string_view to_string(ErrorCode code) noexcept {
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

struct Error {
    ErrorCode code = ErrorCode::Unknown;
    std::string message;
};

template <class... Args>
[[nodiscard]] Error make_error(ErrorCode code, std::format_string<Args...> format, Args&&... args) {
    return Error{.code = code, .message = std::format(format, std::forward<Args>(args)...)};
}

// Value-or-error return type for fallible operations (an expected-like type; C++20 has no std::expected).
//
//   Result<Texture> create_texture(...) {
//       if (bad) return make_error(ErrorCode::InvalidArgument, "width is {}", w);
//       return texture;
//   }
//
// Accessing value() on an error (or error() on a value) is a programmer error and asserts.
template <class T>
class [[nodiscard]] Result {
    static_assert(!std::is_reference_v<T>, "Result<T&> is not supported");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, Error>, "Result<Error> is ambiguous");

public:
    template <class U = T>
        requires(std::constructible_from<T, U &&> && !std::same_as<std::remove_cvref_t<U>, Result> &&
                 !std::same_as<std::remove_cvref_t<U>, Error>)
    constexpr explicit(!std::convertible_to<U &&, T>) Result(U&& value)
        : m_storage(std::in_place_index<0>, std::forward<U>(value)) {}

    constexpr Result(Error error) : m_storage(std::in_place_index<1>, std::move(error)) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return m_storage.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] constexpr T& value() & {
        BYTE_RENDER_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return *std::get_if<0>(&m_storage);
    }
    [[nodiscard]] constexpr const T& value() const& {
        BYTE_RENDER_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return *std::get_if<0>(&m_storage);
    }
    [[nodiscard]] constexpr T&& value() && {
        BYTE_RENDER_ASSERT(has_value(), "Result::value() called on an error: {}", error_message());
        return std::move(*std::get_if<0>(&m_storage));
    }

    [[nodiscard]] constexpr const Error& error() const& {
        BYTE_RENDER_ASSERT(!has_value(), "Result::error() called on a value");
        return *std::get_if<1>(&m_storage);
    }
    [[nodiscard]] constexpr Error&& error() && {
        BYTE_RENDER_ASSERT(!has_value(), "Result::error() called on a value");
        return std::move(*std::get_if<1>(&m_storage));
    }

    template <class U>
    [[nodiscard]] constexpr T value_or(U&& fallback) const& {
        return has_value() ? *std::get_if<0>(&m_storage) : static_cast<T>(std::forward<U>(fallback));
    }
    template <class U>
    [[nodiscard]] constexpr T value_or(U&& fallback) && {
        return has_value() ? std::move(*std::get_if<0>(&m_storage)) : static_cast<T>(std::forward<U>(fallback));
    }

    constexpr T& operator*() & { return value(); }
    constexpr const T& operator*() const& { return value(); }
    constexpr T&& operator*() && { return std::move(*this).value(); }
    constexpr T* operator->() { return &value(); }
    constexpr const T* operator->() const { return &value(); }

private:
    constexpr std::string_view error_message() const noexcept {
        const Error* error = std::get_if<1>(&m_storage);
        return error ? std::string_view{error->message} : std::string_view{};
    }

    std::variant<T, Error> m_storage;
};

// Result of an operation that produces no value: `return {};` on success, `return Error{...};` on failure.
template <>
class [[nodiscard]] Result<void> {
public:
    constexpr Result() noexcept = default;
    constexpr Result(Error error) : m_error(std::move(error)) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return !m_error.has_value(); }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr void value() const {
        BYTE_RENDER_ASSERT(has_value(), "Result<void>::value() called on an error: {}", m_error->message);
    }

    [[nodiscard]] constexpr const Error& error() const& {
        BYTE_RENDER_ASSERT(!has_value(), "Result::error() called on a value");
        return *m_error;
    }
    [[nodiscard]] constexpr Error&& error() && {
        BYTE_RENDER_ASSERT(!has_value(), "Result::error() called on a value");
        return std::move(*m_error);
    }

private:
    std::optional<Error> m_error;
};

} // namespace byte::core
