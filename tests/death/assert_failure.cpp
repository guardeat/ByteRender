// Deliberately fails an assert; CTest checks the printed report (see tests/CMakeLists.txt).
#if !defined(BYTEE_ENABLE_ASSERTS)
    #define BYTEE_ENABLE_ASSERTS 1
#endif
#include <bytee/core/assert.hpp>

int main() {
    const int index = 7;
    const int size = 4;
    BYTEE_ASSERT(index < size, "index {} out of range [0, {})", index, size);
    return 0;
}
