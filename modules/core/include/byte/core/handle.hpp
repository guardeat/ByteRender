#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

namespace byte::core {

inline constexpr uint32_t k_invalid_handle_index = std::numeric_limits<uint32_t>::max();

// Non-owning, trivially copyable reference to an object stored in a Pool: 32-bit slot index + 32-bit generation.
//
// `Tag_` is an empty marker type (usually declared and never defined) that makes handles to different kinds of
// objects distinct types at zero cost: Handle<TextureTag> and Handle<BufferTag> cannot be mixed up.
//
// A default-constructed handle is invalid. The generation detects stale handles: once the object is removed from its
// pool, the slot's generation changes and the old handle no longer matches (see Pool).
template <class Tag_>
class Handle {
private:
    uint32_t index_ = k_invalid_handle_index;
    uint32_t generation_ = 0;

public:
    constexpr Handle() noexcept = default;
    constexpr Handle(uint32_t index, uint32_t generation) noexcept
        : index_(index)
        , generation_(generation) {}

    // True if the handle refers to a slot. It may still be stale; only its pool can tell.
    [[nodiscard]] constexpr bool is_valid() const noexcept { return index_ != k_invalid_handle_index; }
    [[nodiscard]] constexpr uint32_t index() const noexcept { return index_; }
    [[nodiscard]] constexpr uint32_t generation() const noexcept { return generation_; }

    friend constexpr bool operator==(const Handle&, const Handle&) noexcept = default;
};

} // namespace byte::core

// Inside namespace std, plain `byte` names std::byte: always spell ::byte here.
template <class Tag_>
struct std::hash<::byte::core::Handle<Tag_>> {
    [[nodiscard]] std::size_t operator()(const ::byte::core::Handle<Tag_>& handle) const noexcept {
        return std::hash<uint64_t>{}((uint64_t{handle.generation()} << 32u) | handle.index());
    }
};
