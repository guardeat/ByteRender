#pragma once

// Symbol visibility for byte::rhi. See byte/core/export.hpp.

#if defined(BYTE_RENDERER_SHARED)
    #if defined(_WIN32)
        #if defined(BYTE_RENDERER_RHI_BUILDING)
            #define BYTE_RENDERER_RHI_API __declspec(dllexport)
        #else
            #define BYTE_RENDERER_RHI_API __declspec(dllimport)
        #endif
    #else
        #define BYTE_RENDERER_RHI_API __attribute__((visibility("default")))
    #endif
#else
    #define BYTE_RENDERER_RHI_API
#endif
