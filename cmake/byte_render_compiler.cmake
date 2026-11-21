# Compiler setup shared by every ByteRender target: language level, warnings, LTO, sanitizers, formatting.

include(CheckIPOSupported)

# C++20, warnings (as errors unless disabled) and LTO for any ByteRender-owned target (libraries, samples, tests).
function(byte_render_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
        if(BYTE_RENDER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion -Wcast-qual
            -Wnon-virtual-dtor -Woverloaded-virtual -Wold-style-cast
            -Wnull-dereference -Wimplicit-fallthrough -Wformat=2 -Wmisleading-indentation
        )
        # Source locations (asserts, errors, logs) show paths relative to the source tree, never absolute ones.
        target_compile_options(${target} PRIVATE "-fmacro-prefix-map=${PROJECT_SOURCE_DIR}/=")
        if(BYTE_RENDER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(BYTE_RENDER_LTO)
        check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output LANGUAGES CXX)
        if(ipo_supported)
            set_target_properties(${target} PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
                INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
            )
        else()
            message(FATAL_ERROR "BYTE_RENDER_LTO=ON but LTO is not supported: ${ipo_output}")
        endif()
    endif()
endfunction()

# Engine library setup: everything above, plus symbol visibility, export macro and engine-wide definitions.
#   target     - e.g. byte_render_core
#   export_id  - upper-case id used by the module's export.hpp: CORE -> BYTE_RENDER_CORE_API
function(byte_render_configure_library target export_id)
    byte_render_configure_target(${target})
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    if(BYTE_RENDER_BUILD_SHARED)
        target_compile_definitions(${target}
            PUBLIC BYTE_RENDER_SHARED=1
            PRIVATE BYTE_RENDER_${export_id}_BUILDING=1
        )
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endfunction()

# Sanitizers apply to everything in a top-level build (engine, dependencies, samples, tests).
macro(byte_render_enable_sanitizers)
    if(BYTE_RENDER_SANITIZE STREQUAL "address")
        if(MSVC)
            add_compile_options(/fsanitize=address)
        else()
            add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address,undefined)
        endif()
    elseif(BYTE_RENDER_SANITIZE STREQUAL "thread")
        if(MSVC)
            message(FATAL_ERROR "BYTE_RENDER_SANITIZE=thread is not supported by MSVC")
        endif()
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endmacro()

# `format` rewrites all sources with clang-format; `format_check` fails if any file is not formatted.
function(byte_render_add_format_targets)
    find_program(BYTE_RENDER_CLANG_FORMAT clang-format)
    if(NOT BYTE_RENDER_CLANG_FORMAT)
        return()
    endif()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/modules/*.hpp" "${PROJECT_SOURCE_DIR}/modules/*.cpp"
        "${PROJECT_SOURCE_DIR}/samples/*.hpp" "${PROJECT_SOURCE_DIR}/samples/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp" "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/benchmarks/*.hpp" "${PROJECT_SOURCE_DIR}/benchmarks/*.cpp"
    )
    add_custom_target(format
        COMMAND "${BYTE_RENDER_CLANG_FORMAT}" -i ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Formatting sources with clang-format"
        VERBATIM
    )
    add_custom_target(format_check
        COMMAND "${BYTE_RENDER_CLANG_FORMAT}" --dry-run --Werror ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking source formatting"
        VERBATIM
    )
endfunction()
