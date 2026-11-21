#pragma once

// Portable spellings of compiler-specific attributes.

// [[no_unique_address]]: MSVC accepts the standard attribute but ignores it; it needs its own spelling.
#if defined(_MSC_VER) && !defined(__clang__)
    #define BYTEE_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define BYTEE_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
