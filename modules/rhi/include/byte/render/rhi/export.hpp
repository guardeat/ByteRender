#pragma once

// Symbol visibility for byte::render::rhi. Every module has its own macro so that, in shared builds, a module exports
// its own symbols while importing those of the modules it depends on.

#if defined(BYTE_RENDER_SHARED)
    #if defined(_WIN32)
        #if defined(BYTE_RENDER_RHI_BUILDING)
            #define BYTE_RENDER_RHI_API __declspec(dllexport)
        #else
            #define BYTE_RENDER_RHI_API __declspec(dllimport)
        #endif
    #else
        #define BYTE_RENDER_RHI_API __attribute__((visibility("default")))
    #endif
#else
    #define BYTE_RENDER_RHI_API
#endif
