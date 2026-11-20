# Helpers that give every ByteRender target the same compiler setup.

# Warnings for any ByteRender-owned target (libraries, samples, tests).
function(byte_render_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(BYTE_RENDER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Wnull-dereference
        )
        if(BYTE_RENDER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# Common setup for an engine library.
#   target     - CMake target name, e.g. byte_render_core
#   export_id  - upper-case id used by the module's export.hpp, e.g. CORE -> BYTE_RENDER_CORE_API
function(byte_render_configure_library target export_id)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    if(BYTE_RENDER_BUILD_SHARED)
        target_compile_definitions(${target}
            PUBLIC BYTE_RENDER_SHARED=1
            PRIVATE BYTE_RENDER_${export_id}_BUILDING=1
        )
    endif()

    if(BYTE_RENDER_ASSERTS STREQUAL "ON")
        target_compile_definitions(${target} PUBLIC BYTE_RENDER_ENABLE_ASSERTS=1)
    elseif(BYTE_RENDER_ASSERTS STREQUAL "AUTO")
        target_compile_definitions(${target} PUBLIC $<$<CONFIG:Debug>:BYTE_RENDER_ENABLE_ASSERTS=1>)
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()

    byte_render_set_warnings(${target})
endfunction()
