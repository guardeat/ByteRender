// Deliberately looks up a stale handle; CTest checks the assert report (see tests/CMakeLists.txt).
#if !defined(BYTE_RENDER_ENABLE_ASSERTS)
    #define BYTE_RENDER_ENABLE_ASSERTS 1
#endif
#include <byte/core/pool.hpp>

namespace {
struct ItemTag;
}

int main() {
    byte::core::Pool<int, ItemTag> pool{4};
    const auto handle = pool.insert(1);
    pool.remove(handle);
    return pool.get(handle); // use after free
}
