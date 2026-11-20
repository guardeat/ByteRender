#pragma once

#include <concepts>
#include <type_traits>

namespace byte::core {

// An enum class opts into bit-flag behaviour with BYTE_RENDER_ENABLE_FLAGS(Enum), placed in the
// same namespace as the enum (found through argument-dependent lookup).
template <class E>
concept FlagEnum = std::is_enum_v<E> && std::is_unsigned_v<std::underlying_type_t<E>> && requires(E e) {
    { byte_render_enable_flags(e) } -> std::same_as<bool>;
};

// Type-safe set of bits from a flag enum:
//
//   enum class TextureUsage : uint32_t { Sampled = 1 << 0, Storage = 1 << 1, ColorAttachment = 1 << 2 };
//   BYTE_RENDER_ENABLE_FLAGS(TextureUsage)
//
//   Flags<TextureUsage> usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
//   if (usage.has(TextureUsage::Sampled)) { ... }
template <FlagEnum E>
class Flags {
public:
    using Bits = std::underlying_type_t<E>;

    constexpr Flags() noexcept = default;
    constexpr Flags(E bit) noexcept : m_bits(static_cast<Bits>(bit)) {}
    constexpr explicit Flags(Bits bits) noexcept : m_bits(bits) {}

    [[nodiscard]] constexpr Bits bits() const noexcept { return m_bits; }
    [[nodiscard]] constexpr bool none() const noexcept { return m_bits == 0; }
    [[nodiscard]] constexpr bool any() const noexcept { return m_bits != 0; }
    constexpr explicit operator bool() const noexcept { return any(); }

    // True if every bit of `flags` is set.
    [[nodiscard]] constexpr bool has(Flags flags) const noexcept { return (m_bits & flags.m_bits) == flags.m_bits; }
    // True if at least one bit of `flags` is set.
    [[nodiscard]] constexpr bool has_any(Flags flags) const noexcept { return (m_bits & flags.m_bits) != 0; }

    constexpr Flags& operator|=(Flags other) noexcept {
        m_bits = static_cast<Bits>(m_bits | other.m_bits);
        return *this;
    }
    constexpr Flags& operator&=(Flags other) noexcept {
        m_bits = static_cast<Bits>(m_bits & other.m_bits);
        return *this;
    }
    constexpr Flags& operator^=(Flags other) noexcept {
        m_bits = static_cast<Bits>(m_bits ^ other.m_bits);
        return *this;
    }

    friend constexpr Flags operator|(Flags a, Flags b) noexcept { return a |= b; }
    friend constexpr Flags operator&(Flags a, Flags b) noexcept { return a &= b; }
    friend constexpr Flags operator^(Flags a, Flags b) noexcept { return a ^= b; }
    friend constexpr bool operator==(Flags, Flags) noexcept = default;

private:
    Bits m_bits = 0;
};

} // namespace byte::core

// Must be used in the namespace that declares `Enum`.
#define BYTE_RENDER_ENABLE_FLAGS(Enum)                                                                     \
    [[maybe_unused]] constexpr bool byte_render_enable_flags(Enum) noexcept {                              \
        return true;                                                                                       \
    }                                                                                                      \
    [[maybe_unused]] constexpr ::byte::core::Flags<Enum> operator|(Enum a, Enum b) noexcept {              \
        return ::byte::core::Flags<Enum>(a) | b;                                                           \
    }                                                                                                      \
    [[maybe_unused]] constexpr ::byte::core::Flags<Enum> operator&(Enum a, Enum b) noexcept {              \
        return ::byte::core::Flags<Enum>(a) & b;                                                           \
    }
