#pragma once

// Portable spellings of compiler-specific attributes.

// [[no_unique_address]]: the MSVC ABI (MSVC and clang-cl) ignores the standard attribute; it needs its own spelling.
#if defined(_MSC_VER)
    #define BYTEE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define BYTEE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

// Keeps a type's symbols out of a shared library's exports even when it is nested in an exported class (nested types
// inherit their parent's visibility on GCC/Clang). Used for backend-defined opaque `Impl` types. On Windows only
// explicitly exported symbols leave a DLL, so nothing is needed there.
#if defined(_WIN32)
    #define BYTEE_HIDDEN
#else
    #define BYTEE_HIDDEN __attribute__((visibility("hidden")))
#endif
