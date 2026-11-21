#pragma once

// Typed deferred destruction of GPU objects (architecture §6).
//
// When the CPU owner of a GPU object goes away, the object is pushed here with every queue's *last submitted*
// timeline value at that moment. It is destroyed once every queue has completed at least those values, i.e. once no
// GPU work that could reference it is still in flight.

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace bytee::render::rhi::detail {

template <std::size_t QueueCount_>
using QueueValues = std::array<uint64_t, QueueCount_>;

// True if every queue has completed at least the required value.
template <std::size_t QueueCount_>
[[nodiscard]] constexpr bool is_reached(const QueueValues<QueueCount_>& required,
                                        const QueueValues<QueueCount_>& completed) noexcept {
    for (std::size_t queue = 0; queue < QueueCount_; ++queue) {
        if (completed[queue] < required[queue]) {
            return false;
        }
    }
    return true;
}

// Thread-safe FIFO of retired items of one type. The destroy function is supplied at collection time, so items are
// plain data (no std::function, no type erasure). Destroy functions run outside the lock, one item at a time — no
// allocation while collecting, and a destroy function may retire further items.
//
// Processing stops at the first item that is not ready. Values are non-decreasing in push order in practice; if
// concurrent pushes interleave, a later-ready item can delay an earlier-ready one behind it. That is conservative
// (never destroys too early), never incorrect.
template <class Item_, std::size_t QueueCount_>
class RetireList {
public:
    using Values = QueueValues<QueueCount_>;

private:
    struct Entry {
        Values retire_after{};
        Item_ item;
    };

    mutable std::mutex mutex_;
    std::deque<Entry> entries_;

public:
    void push(const Values& retire_after, Item_ item) {
        const std::scoped_lock lock{mutex_};
        entries_.push_back(Entry{.retire_after = retire_after, .item = std::move(item)});
    }

    // Destroys every ready item, in push order. Returns how many were destroyed.
    template <class Destroy_>
        requires std::invocable<Destroy_&, Item_&>
    std::size_t collect(const Values& completed, Destroy_&& destroy) {
        std::size_t destroyed = 0;
        while (std::optional<Item_> item =
                   pop_if([&](const Entry& entry) { return is_reached(entry.retire_after, completed); })) {
            destroy(*item);
            ++destroyed;
        }
        return destroyed;
    }

    // Destroys every item regardless of GPU progress — only valid when the GPU is idle. Includes items retired by
    // the destroy function itself.
    template <class Destroy_>
        requires std::invocable<Destroy_&, Item_&>
    std::size_t drain(Destroy_&& destroy) {
        std::size_t destroyed = 0;
        while (std::optional<Item_> item = pop_if([](const Entry&) { return true; })) {
            destroy(*item);
            ++destroyed;
        }
        return destroyed;
    }

    [[nodiscard]] std::size_t size() const {
        const std::scoped_lock lock{mutex_};
        return entries_.size();
    }

private:
    template <class Predicate_>
    std::optional<Item_> pop_if(Predicate_&& predicate) {
        const std::scoped_lock lock{mutex_};
        if (entries_.empty() || !predicate(entries_.front())) {
            return std::nullopt;
        }
        std::optional<Item_> item{std::move(entries_.front().item)};
        entries_.pop_front();
        return item;
    }
};

} // namespace bytee::render::rhi::detail
