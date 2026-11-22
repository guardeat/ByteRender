# Compiler setup shared by every Bytee target: language level, warnings, LTO, sanitizers, formatting.

include(CheckIPOSupported)

# Checked once per configure (each check compiles a test project).
if(BYTEE_LTO)
    check_ipo_supported(RESULT bytee_ipo_supported OUTPUT bytee_ipo_output LANGUAGES CXX)
    if(NOT bytee_ipo_supported)
        message(FATAL_ERROR "BYTEE_LTO=ON but LTO is not supported: ${bytee_ipo_output}")
    endif()
endif()

# C++20, warnings (as errors unless disabled) and LTO for any Bytee-owned target (libraries, samples, tests).
function(bytee_configure_target target)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
        # Source locations (asserts, errors, logs) relative to the source tree, like -fmacro-prefix-map below.
        file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/" source_prefix)
        target_compile_options(${target} PRIVATE "/d1trimfile:${source_prefix}")
        if(BYTEE_WARNINGS_AS_ERRORS)
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
        if(BYTEE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(BYTEE_LTO)
        set_target_properties(${target} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
        )
    endif()
endfunction()

# Engine library setup: everything above, plus symbol visibility, export macro and engine-wide definitions.
#   target     - e.g. bytee_core
#   export_id  - upper-case id used by the module's export.hpp: CORE -> BYTEE_CORE_API
function(bytee_configure_library target export_id)
    bytee_configure_target(${target})
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    if(BYTEE_BUILD_SHARED)
        target_compile_definitions(${target}
            PUBLIC BYTEE_SHARED=1
            PRIVATE BYTEE_${export_id}_BUILDING=1
        )
    endif()

    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endfunction()

# Sanitizers apply to everything in a top-level build (engine, dependencies, samples, tests).
macro(bytee_enable_sanitizers)
    if(BYTEE_SANITIZE STREQUAL "address")
        if(MSVC)
            # ASan is incompatible with run-time checks (/RTC1, in CMake's Debug flags) and incremental linking.
            string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
            string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
            add_compile_options(/fsanitize=address)
            add_link_options(/INCREMENTAL:NO)
        else()
            add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
            add_link_options(-fsanitize=address,undefined)
        endif()
    elseif(BYTEE_SANITIZE STREQUAL "thread")
        if(MSVC)
            message(FATAL_ERROR "BYTEE_SANITIZE=thread is not supported by MSVC")
        endif()
        add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
        add_link_options(-fsanitize=thread)
    endif()
endmacro()

# `format` rewrites all sources with clang-format; `format_check` fails if any file is not formatted.
function(bytee_add_format_targets)
    find_program(BYTEE_CLANG_FORMAT clang-format)
    if(NOT BYTEE_CLANG_FORMAT)
        return()
    endif()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/modules/*.hpp" "${PROJECT_SOURCE_DIR}/modules/*.cpp"
        "${PROJECT_SOURCE_DIR}/samples/*.hpp" "${PROJECT_SOURCE_DIR}/samples/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp" "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/benchmarks/*.hpp" "${PROJECT_SOURCE_DIR}/benchmarks/*.cpp"
    )
    add_custom_target(format
        COMMAND "${BYTEE_CLANG_FORMAT}" -i ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Formatting sources with clang-format"
        VERBATIM
    )
    add_custom_target(format_check
        COMMAND "${BYTEE_CLANG_FORMAT}" --dry-run --Werror ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking source formatting"
        VERBATIM
    )
endfunction()
