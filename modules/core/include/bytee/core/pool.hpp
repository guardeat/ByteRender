#pragma once

#include <bytee/core/assert.hpp>
#include <bytee/core/handle.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace bytee::core {

namespace detail {
// Pool slot generations are odd while the slot holds a value and even while it is free.
[[nodiscard]] constexpr bool is_alive_generation(uint32_t generation) noexcept {
    return (generation & 1u) != 0;
}
} // namespace detail

// Generational object pool: values live in fixed slots and are referenced by Handle<Tag_>.
//
// - O(1) insert, remove and lookup; no hashing, no per-object allocation.
// - Fixed maximum capacity, chosen at construction. Storage is allocated lazily in chunks of k_chunk_size slots and
//   never moves: references returned by get() stay valid until that object is removed.
// - Each slot has a generation: odd while alive, even while free. Removing an object bumps it, so stale handles are
//   detected (contains()/try_get(); get() asserts in Debug and is unchecked in Release).
// - Thread safety: not internally synchronized. insert()/remove()/clear() need external synchronization; lookups
//   may run concurrently with insert()/remove() of *other* slots.
template <class Value_, class Tag_>
class Pool {
public:
    using HandleType = Handle<Tag_>;

    static constexpr uint32_t k_chunk_size = 256; // power of two

private:
    static constexpr uint32_t k_chunk_shift = 8;
    static_assert((1u << k_chunk_shift) == k_chunk_size);

    struct Chunk {
        uint32_t generations[k_chunk_size] = {};
        alignas(Value_) std::byte storage[k_chunk_size][sizeof(Value_)];
    };

    std::unique_ptr<std::unique_ptr<Chunk>[]> chunks_; // fixed-size table; entries allocated on demand
    std::vector<uint32_t> free_indices_;               // LIFO: recently freed (cache-warm) slots are reused first
    uint32_t capacity_ = 0;
    uint32_t high_water_ = 0; // slots [0, high_water_) have been used at least once
    uint32_t size_ = 0;

public:
    explicit Pool(uint32_t capacity)
        : chunks_(std::make_unique<std::unique_ptr<Chunk>[]>((capacity + k_chunk_size - 1) / k_chunk_size))
        , capacity_(capacity) {
        BYTEE_ASSERT(capacity < k_invalid_handle_index, "pool capacity {} is too large", capacity);
    }

    ~Pool() { clear(); }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&& other) noexcept
        : chunks_(std::move(other.chunks_))
        , free_indices_(std::move(other.free_indices_))
        , capacity_(std::exchange(other.capacity_, 0))
        , high_water_(std::exchange(other.high_water_, 0))
        , size_(std::exchange(other.size_, 0)) {}
    Pool& operator=(Pool&& other) noexcept {
        if (this != &other) {
            clear();
            chunks_ = std::move(other.chunks_);
            free_indices_ = std::move(other.free_indices_);
            capacity_ = std::exchange(other.capacity_, 0);
            high_water_ = std::exchange(other.high_water_, 0);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    [[nodiscard]] uint32_t size() const noexcept { return size_; }
    [[nodiscard]] uint32_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    // Constructs a value in a free slot. Returns an invalid handle if the pool is full.
    template <class... Args_>
        requires std::constructible_from<Value_, Args_...>
    [[nodiscard]] HandleType insert(Args_&&... args) {
        uint32_t index = 0;
        if (!free_indices_.empty()) {
            index = free_indices_.back();
            free_indices_.pop_back();
        } else if (high_water_ < capacity_) {
            index = high_water_++;
            std::unique_ptr<Chunk>& chunk = chunks_[index >> k_chunk_shift];
            if (!chunk) {
                chunk = std::make_unique<Chunk>();
            }
        } else {
            return HandleType{};
        }

        std::construct_at(value_pointer(index), std::forward<Args_>(args)...);
        uint32_t& generation = generation_ref(index);
        ++generation; // even -> odd: alive
        ++size_;
        return HandleType{index, generation};
    }

    // Destroys the value and frees its slot. Removing a stale or invalid handle is a programmer error (asserts in
    // Debug; returns false in Release).
    bool remove(HandleType handle) {
        if (!contains(handle)) {
            BYTEE_ASSERT(false, "Pool::remove: stale or invalid handle (index {}, generation {})", handle.index(),
                         handle.generation());
            return false;
        }
        const uint32_t index = handle.index();
        std::destroy_at(value_pointer(index));
        ++generation_ref(index); // odd -> even: free
        free_indices_.push_back(index);
        --size_;
        return true;
    }

    // Destroys every value. Outstanding handles become stale.
    void clear() noexcept {
        for (uint32_t index = 0; index < high_water_; ++index) {
            uint32_t& generation = generation_ref(index);
            if (detail::is_alive_generation(generation)) {
                std::destroy_at(value_pointer(index));
                ++generation;
                free_indices_.push_back(index);
            }
        }
        size_ = 0;
    }

    [[nodiscard]] bool contains(HandleType handle) const noexcept {
        return handle.index() < high_water_ && detail::is_alive_generation(handle.generation()) &&
               generation_ref(handle.index()) == handle.generation();
    }

    // Unchecked in Release: the handle must be alive.
    [[nodiscard]] Value_& get(HandleType handle) noexcept {
        BYTEE_ASSERT(contains(handle), "Pool::get: stale or invalid handle (index {}, generation {})", handle.index(),
                     handle.generation());
        return *value_pointer(handle.index());
    }
    [[nodiscard]] const Value_& get(HandleType handle) const noexcept {
        BYTEE_ASSERT(contains(handle), "Pool::get: stale or invalid handle (index {}, generation {})", handle.index(),
                     handle.generation());
        return *value_pointer(handle.index());
    }

    // Checked in every build: nullptr for stale or invalid handles.
    [[nodiscard]] Value_* try_get(HandleType handle) noexcept {
        return contains(handle) ? value_pointer(handle.index()) : nullptr;
    }
    [[nodiscard]] const Value_* try_get(HandleType handle) const noexcept {
        return contains(handle) ? value_pointer(handle.index()) : nullptr;
    }

    // Calls function(handle, value) for every live value, in slot order.
    template <class Function_>
        requires std::invocable<Function_&, HandleType, Value_&>
    void for_each(Function_&& function) {
        for (uint32_t index = 0; index < high_water_; ++index) {
            const uint32_t generation = generation_ref(index);
            if (detail::is_alive_generation(generation)) {
                function(HandleType{index, generation}, *value_pointer(index));
            }
        }
    }
    template <class Function_>
        requires std::invocable<Function_&, HandleType, const Value_&>
    void for_each(Function_&& function) const {
        for (uint32_t index = 0; index < high_water_; ++index) {
            const uint32_t generation = generation_ref(index);
            if (detail::is_alive_generation(generation)) {
                function(HandleType{index, generation}, *value_pointer(index));
            }
        }
    }

private:
    [[nodiscard]] uint32_t& generation_ref(uint32_t index) const noexcept {
        return chunks_[index >> k_chunk_shift]->generations[index & (k_chunk_size - 1)];
    }

    [[nodiscard]] Value_* value_pointer(uint32_t index) const noexcept {
        std::byte* bytes = chunks_[index >> k_chunk_shift]->storage[index & (k_chunk_size - 1)];
        return std::launder(reinterpret_cast<Value_*>(bytes));
    }
};

} // namespace bytee::core
