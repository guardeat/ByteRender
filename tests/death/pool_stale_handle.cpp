// Deliberately looks up a stale handle; CTest checks the assert report (see tests/CMakeLists.txt).
#if !defined(BYTEE_ENABLE_ASSERTS)
    #define BYTEE_ENABLE_ASSERTS 1
#endif
#include <bytee/core/pool.hpp>

namespace {
struct ItemTag;
}

int main() {
    bytee::core::Pool<int, ItemTag> pool{4};
    const auto handle = pool.insert(1);
    pool.remove(handle);
    return pool.get(handle); // use after free
}
