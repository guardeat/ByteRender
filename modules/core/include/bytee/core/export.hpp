#pragma once

// Symbol visibility for bytee::core. Every module has its own macro so that, in shared builds, a module exports its
// own symbols while importing those of the modules it depends on.

#if defined(BYTEE_SHARED)
    #if defined(_WIN32)
        #if defined(BYTEE_CORE_BUILDING)
            #define BYTEE_CORE_API __declspec(dllexport)
        #else
            #define BYTEE_CORE_API __declspec(dllimport)
        #endif
    #else
        #define BYTEE_CORE_API __attribute__((visibility("default")))
    #endif
#else
    #define BYTEE_CORE_API
#endif
