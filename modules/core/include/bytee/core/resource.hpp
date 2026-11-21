#pragma once

#include <bytee/core/compiler.hpp>
#include <bytee/core/handle.hpp>

#include <concepts>
#include <type_traits>
#include <utility>

namespace bytee::core {

// A deleter releases the object behind a handle. It must not throw (it runs in destructors) and must be cheap to
// move. It decides the destruction policy: GPU resources retire to their device (deferred), others delete at once.
template <class Deleter_, class Tag_>
concept HandleDeleter = std::is_nothrow_invocable_v<Deleter_&, Handle<Tag_>> &&
                        std::is_nothrow_move_constructible_v<Deleter_> && std::is_nothrow_move_assignable_v<Deleter_>;

// Move-only RAII owner of a handle. Publicly derives from Handle<Tag_>, so an owner can be passed wherever a handle
// is expected; copying the handle out is intended (it is a non-owning view). Handle has no virtual functions and is a
// value type, never deleted through a base pointer, so the public inheritance is safe.
//
// With an empty deleter a Resource is exactly as large as a Handle (8 bytes).
template <class Tag_, class Deleter_>
    requires HandleDeleter<Deleter_, Tag_>
class Resource : public Handle<Tag_> {
public:
    using HandleType = Handle<Tag_>;

private:
    BYTEE_NO_UNIQUE_ADDRESS Deleter_ deleter_;

public:
    // Empty (invalid) resource.
    constexpr Resource() noexcept
        requires std::is_nothrow_default_constructible_v<Deleter_>
    = default;

    constexpr Resource(HandleType handle, Deleter_ deleter) noexcept
        : HandleType(handle)
        , deleter_(std::move(deleter)) {}

    ~Resource() { reset(); }

    Resource(Resource&& other) noexcept
        : HandleType(other.release())
        , deleter_(std::move(other.deleter_)) {}

    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            reset();
            HandleType::operator=(other.release());
            deleter_ = std::move(other.deleter_);
        }
        return *this;
    }

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;

    [[nodiscard]] constexpr HandleType handle() const noexcept { return *this; }
    [[nodiscard]] constexpr const Deleter_& deleter() const noexcept { return deleter_; }

    // Gives up ownership without deleting; the resource becomes empty.
    [[nodiscard]] constexpr HandleType release() noexcept {
        const HandleType handle = *this;
        HandleType::operator=(HandleType{});
        return handle;
    }

    // Deletes the owned object now (if any); the resource becomes empty.
    void reset() noexcept {
        if (this->is_valid()) {
            deleter_(release());
        }
    }
};

} // namespace bytee::core
