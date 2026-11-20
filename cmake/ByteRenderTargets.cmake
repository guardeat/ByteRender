# Helpers that give every ByteRender target the same compiler setup.

# Warnings for any ByteRender-owned target (libraries, samples, tests).
function(byte_renderer_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(BYTE_RENDERER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Wnull-dereference
        )
        if(BYTE_RENDERER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Common setup for an engine library.
#   target     - CMake target name, e.g. byte_renderer_core
#   export_id  - upper-case id used by the module's Export.h, e.g. CORE -> BYTE_RENDERER_CORE_API
function(byte_renderer_configure_library target export_id)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    if(BYTE_RENDERER_BUILD_SHARED)
        target_compile_definitions(${target}
            PUBLIC BYTE_RENDERER_SHARED=1
            PRIVATE BYTE_RENDERER_${export_id}_BUILDING=1
        )
    endif()

    if(BYTE_RENDERER_ASSERTS STREQUAL "ON")
        target_compile_definitions(${target} PUBLIC BYTE_RENDERER_ENABLE_ASSERTS=1)
    elseif(BYTE_RENDERER_ASSERTS STREQUAL "AUTO")
        target_compile_definitions(${target} PUBLIC $<$<CONFIG:Debug>:BYTE_RENDERER_ENABLE_ASSERTS=1>)
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()

    byte_renderer_set_warnings(${target})
endfunction()
