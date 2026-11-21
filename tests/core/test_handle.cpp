#include <bytee/core/handle.hpp>

#include <doctest/doctest.h>

#include <type_traits>
#include <unordered_set>

namespace {
struct TextureTag;
struct BufferTag;
} // namespace

using bytee::core::Handle;
using TextureHandle = Handle<TextureTag>;
using BufferHandle = Handle<BufferTag>;

static_assert(sizeof(TextureHandle) == 8);
static_assert(std::is_trivially_copyable_v<TextureHandle>);
static_assert(!std::is_convertible_v<TextureHandle, BufferHandle>, "different tags must not mix");
static_assert(!std::is_constructible_v<BufferHandle, TextureHandle>, "different tags must not mix");

TEST_CASE("A default handle is invalid") {
    constexpr TextureHandle handle;
    CHECK_FALSE(handle.is_valid());
    CHECK(handle.index() == bytee::core::k_invalid_handle_index);
}

TEST_CASE("Handles compare index and generation") {
    constexpr TextureHandle a{3, 1};
    CHECK(a.is_valid());
    CHECK(a == TextureHandle{3, 1});
    CHECK_FALSE(a == TextureHandle{3, 3}); // same slot, different generation: a different object
    CHECK_FALSE(a == TextureHandle{4, 1});
}

TEST_CASE("Handles are hashable") {
    std::unordered_set<TextureHandle> set;
    set.insert(TextureHandle{1, 1});
    set.insert(TextureHandle{1, 3});
    set.insert(TextureHandle{1, 1});
    CHECK(set.size() == 2);
}
