#pragma once

// Symbol visibility for the render graph module (namespace bytee::render). Every module has its own macro so that, in
// shared builds, a module exports its own symbols while importing those of the modules it depends on.

#if defined(BYTEE_SHARED)
    #if defined(_WIN32)
        #if defined(BYTEE_GRAPH_BUILDING)
            #define BYTEE_GRAPH_API __declspec(dllexport)
        #else
            #define BYTEE_GRAPH_API __declspec(dllimport)
        #endif
    #else
        #define BYTEE_GRAPH_API __attribute__((visibility("default")))
    #endif
#else
    #define BYTEE_GRAPH_API
#endif
