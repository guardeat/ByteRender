// Deliberately fails an assert; CTest checks the printed report (see tests/CMakeLists.txt).
#if !defined(BYTE_RENDER_ENABLE_ASSERTS)
    #define BYTE_RENDER_ENABLE_ASSERTS 1
#endif
#include <byte/core/assert.hpp>

int main() {
    const int index = 7;
    const int size = 4;
    BYTE_RENDER_ASSERT(index < size, "index {} out of range [0, {})", index, size);
    return 0;
}
