# GLSL -> SPIR-V at build time, embedded into C++ as a constexpr array (no shader files needed at runtime).
#
#   bytee_add_shaders(my_target SOURCES shaders/triangle.vert shaders/solid.frag)
#   #include "triangle.vert.hpp"   ->   shaders::triangle_vert   (std::array<std::uint32_t, N>)
#
# Stage from the extension (.vert, .frag, .comp). Debug builds embed debug info (-g). Includes are tracked through
# glslang depfiles, so editing an included file recompiles its users.

find_program(BYTEE_GLSLANG_VALIDATOR glslangValidator HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")

function(bytee_add_shaders target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES")
    if(NOT BYTEE_GLSLANG_VALIDATOR)
        message(FATAL_ERROR "glslangValidator not found (install the Vulkan SDK, or glslang via Homebrew on macOS)")
    endif()

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/bytee_shaders")
    foreach(source IN LISTS arg_SOURCES)
        get_filename_component(source_path "${source}" ABSOLUTE)
        get_filename_component(file_name "${source}" NAME)
        string(MAKE_C_IDENTIFIER "${file_name}" variable_name) # triangle.vert -> triangle_vert
        set(spirv "${output_dir}/${file_name}.spv")
        set(header "${output_dir}/${file_name}.hpp")
        set(depfile "${output_dir}/${file_name}.d")

        add_custom_command(
            OUTPUT "${header}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
            COMMAND "${BYTEE_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3 --quiet $<$<CONFIG:Debug>:-g>
                    --depfile "${depfile}" -o "${spirv}" "${source_path}"
            COMMAND "${CMAKE_COMMAND}" -DINPUT=${spirv} -DOUTPUT=${header} -DNAME=${variable_name}
                    -P "${PROJECT_SOURCE_DIR}/cmake/bytee_embed_spirv.cmake"
            DEPENDS "${source_path}" "${PROJECT_SOURCE_DIR}/cmake/bytee_embed_spirv.cmake"
            DEPFILE "${depfile}"
            COMMENT "Compiling shader ${file_name}"
            VERBATIM
            COMMAND_EXPAND_LISTS # the Debug-only "-g" must vanish in other configurations, not become an empty argument
        )
        target_sources(${target} PRIVATE "${header}")
    endforeach()
    target_include_directories(${target} PRIVATE "${output_dir}")
endfunction()
