#include <bytee/core/resource.hpp>

#include <doctest/doctest.h>

#include <type_traits>
#include <utility>
#include <vector>

namespace {

struct ThingTag;
using ThingHandle = bytee::core::Handle<ThingTag>;

// Records every deletion.
struct RecordingDeleter {
    std::vector<ThingHandle>* deleted = nullptr;
    void operator()(ThingHandle handle) noexcept { deleted->push_back(handle); }
};
using Thing = bytee::core::Resource<ThingTag, RecordingDeleter>;

struct EmptyDeleter {
    void operator()(ThingHandle) noexcept {}
};

struct ThrowingDeleter {
    void operator()(ThingHandle) {}
};

uint32_t index_of(ThingHandle handle) {
    return handle.index();
}

} // namespace

static_assert(sizeof(bytee::core::Resource<ThingTag, EmptyDeleter>) == sizeof(ThingHandle),
              "an empty deleter must cost nothing");
static_assert(sizeof(Thing) == 16);
static_assert(!std::is_copy_constructible_v<Thing> && !std::is_copy_assignable_v<Thing>);
static_assert(std::is_nothrow_move_constructible_v<Thing> && std::is_nothrow_move_assignable_v<Thing>);
static_assert(bytee::core::HandleDeleter<EmptyDeleter, ThingTag>);
static_assert(!bytee::core::HandleDeleter<ThrowingDeleter, ThingTag>, "deleters must be noexcept");

TEST_CASE("A resource deletes its handle exactly once when destroyed") {
    std::vector<ThingHandle> deleted;
    {
        const Thing thing{ThingHandle{5, 1}, RecordingDeleter{&deleted}};
        CHECK(thing.is_valid());
        CHECK(index_of(thing) == 5); // a resource is usable wherever a handle is expected
    }
    REQUIRE(deleted.size() == 1);
    CHECK(deleted[0] == ThingHandle{5, 1});
}

TEST_CASE("Moving transfers ownership") {
    std::vector<ThingHandle> deleted;
    {
        Thing first{ThingHandle{1, 1}, RecordingDeleter{&deleted}};
        Thing second = std::move(first);
        CHECK_FALSE(first.is_valid()); // NOLINT(bugprone-use-after-move): checking the moved-from state
        CHECK(second.handle() == ThingHandle{1, 1});
        CHECK(deleted.empty());
    }
    CHECK(deleted.size() == 1);
}

TEST_CASE("Move assignment deletes the previous handle") {
    std::vector<ThingHandle> deleted;
    Thing target{ThingHandle{1, 1}, RecordingDeleter{&deleted}};
    Thing source{ThingHandle{2, 1}, RecordingDeleter{&deleted}};

    target = std::move(source);
    REQUIRE(deleted.size() == 1);
    CHECK(deleted[0] == ThingHandle{1, 1});
    CHECK(target.handle() == ThingHandle{2, 1});

    auto& self = target;
    target = std::move(self); // self-move is a no-op
    CHECK(target.handle() == ThingHandle{2, 1});
    CHECK(deleted.size() == 1);
}

TEST_CASE("release() gives up ownership, reset() deletes now") {
    std::vector<ThingHandle> deleted;
    Thing released{ThingHandle{7, 1}, RecordingDeleter{&deleted}};
    const ThingHandle handle = released.release();
    CHECK(handle == ThingHandle{7, 1});
    CHECK_FALSE(released.is_valid());

    Thing reset{ThingHandle{8, 1}, RecordingDeleter{&deleted}};
    reset.reset();
    CHECK_FALSE(reset.is_valid());
    REQUIRE(deleted.size() == 1);
    CHECK(deleted[0] == ThingHandle{8, 1});
}

TEST_CASE("An empty resource deletes nothing") {
    const bytee::core::Resource<ThingTag, EmptyDeleter> empty;
    CHECK_FALSE(empty.is_valid());
}
