#pragma once

// Symbol visibility for byte::core. Each module has its own macro so that, in shared builds,
// a module exports its own symbols while importing those of the modules it depends on.

#if defined(BYTE_RENDERER_SHARED)
    #if defined(_WIN32)
        #if defined(BYTE_RENDERER_CORE_BUILDING)
            #define BYTE_RENDERER_CORE_API __declspec(dllexport)
        #else
            #define BYTE_RENDERER_CORE_API __declspec(dllimport)
        #endif
    #else
        #define BYTE_RENDERER_CORE_API __attribute__((visibility("default")))
    #endif
#else
    #define BYTE_RENDERER_CORE_API
#endif
