#pragma once

#include <concepts>
#include <type_traits>

namespace byte::core {

// An enum class opts into bit-flag behaviour with BYTE_RENDER_ENABLE_FLAGS(Enum), written in the same namespace as
// the enum (found through argument-dependent lookup). The underlying type must be unsigned.
template <class Enum_>
concept FlagEnum = std::is_enum_v<Enum_> && std::is_unsigned_v<std::underlying_type_t<Enum_>> && requires(Enum_ bit) {
    { byte_render_enable_flags(bit) } -> std::same_as<bool>;
};

// Type-safe set of bits from a flag enum:
//
//   enum class TextureUsage : uint32_t { Sampled = 1u << 0, Storage = 1u << 1, ColorAttachment = 1u << 2 };
//   BYTE_RENDER_ENABLE_FLAGS(TextureUsage)
//
//   Flags<TextureUsage> usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
//   if (usage.has(TextureUsage::Sampled)) { ... }
template <FlagEnum Enum_>
class Flags {
public:
    using Bits = std::underlying_type_t<Enum_>;

private:
    Bits bits_ = 0;

public:
    constexpr Flags() noexcept = default;
    constexpr Flags(Enum_ bit) noexcept
        : bits_(static_cast<Bits>(bit)) {}
    constexpr explicit Flags(Bits bits) noexcept
        : bits_(bits) {}

    [[nodiscard]] constexpr Bits bits() const noexcept { return bits_; }
    [[nodiscard]] constexpr bool none() const noexcept { return bits_ == 0; }
    [[nodiscard]] constexpr bool any() const noexcept { return bits_ != 0; }
    constexpr explicit operator bool() const noexcept { return any(); }

    // True if every bit of `flags` is set.
    [[nodiscard]] constexpr bool has(Flags flags) const noexcept { return (bits_ & flags.bits_) == flags.bits_; }
    // True if at least one bit of `flags` is set.
    [[nodiscard]] constexpr bool has_any(Flags flags) const noexcept { return (bits_ & flags.bits_) != 0; }
    // These flags with every bit of `flags` cleared.
    [[nodiscard]] constexpr Flags without(Flags flags) const noexcept {
        return Flags{static_cast<Bits>(bits_ & static_cast<Bits>(~flags.bits_))};
    }

    constexpr Flags& operator|=(Flags other) noexcept {
        bits_ = static_cast<Bits>(bits_ | other.bits_);
        return *this;
    }
    constexpr Flags& operator&=(Flags other) noexcept {
        bits_ = static_cast<Bits>(bits_ & other.bits_);
        return *this;
    }
    constexpr Flags& operator^=(Flags other) noexcept {
        bits_ = static_cast<Bits>(bits_ ^ other.bits_);
        return *this;
    }

    friend constexpr Flags operator|(Flags a, Flags b) noexcept { return a |= b; }
    friend constexpr Flags operator&(Flags a, Flags b) noexcept { return a &= b; }
    friend constexpr Flags operator^(Flags a, Flags b) noexcept { return a ^= b; }
    friend constexpr bool operator==(Flags, Flags) noexcept = default;
};

} // namespace byte::core

// Enables Flags<Enum> and `Enum | Enum`, `Enum & Enum`. Must be used in the namespace that declares `Enum`.
#define BYTE_RENDER_ENABLE_FLAGS(Enum)                                                                                 \
    [[maybe_unused]] constexpr bool byte_render_enable_flags(Enum) noexcept {                                          \
        return true;                                                                                                   \
    }                                                                                                                  \
    [[maybe_unused]] constexpr ::byte::core::Flags<Enum> operator|(Enum a, Enum b) noexcept {                          \
        return ::byte::core::Flags<Enum>(a) | b;                                                                       \
    }                                                                                                                  \
    [[maybe_unused]] constexpr ::byte::core::Flags<Enum> operator&(Enum a, Enum b) noexcept {                          \
        return ::byte::core::Flags<Enum>(a) & b;                                                                       \
    }
