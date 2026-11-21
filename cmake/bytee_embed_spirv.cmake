# Writes a SPIR-V binary as a C++ header: inline constexpr std::array<std::uint32_t, N> shaders::<NAME>.
#   cmake -DINPUT=<file.spv> -DOUTPUT=<file.hpp> -DNAME=<identifier> -P bytee_embed_spirv.cmake

file(READ "${INPUT}" hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR remainder "${hex_length} % 8")
if(hex_length EQUAL 0 OR NOT remainder EQUAL 0)
    message(FATAL_ERROR "${INPUT} is not a valid SPIR-V binary (size must be a non-zero multiple of 4 bytes)")
endif()
math(EXPR word_count "${hex_length} / 8")

set(words "")
math(EXPR last "${word_count} - 1")
foreach(index RANGE 0 ${last})
    math(EXPR offset "${index} * 8")
    string(SUBSTRING "${hex}" ${offset} 8 word)
    # SPIR-V files are little-endian: bytes b0 b1 b2 b3 form the word 0xb3b2b1b0.
    string(SUBSTRING "${word}" 0 2 b0)
    string(SUBSTRING "${word}" 2 2 b1)
    string(SUBSTRING "${word}" 4 2 b2)
    string(SUBSTRING "${word}" 6 2 b3)
    math(EXPR column "${index} % 8")
    if(column EQUAL 0)
        string(APPEND words "\n   ")
    endif()
    string(APPEND words " 0x${b3}${b2}${b1}${b0}u,")
endforeach()

file(WRITE "${OUTPUT}" "// Generated from ${INPUT} by bytee_embed_spirv.cmake. Do not edit.
#pragma once

#include <array>
#include <cstdint>

namespace shaders {

inline constexpr std::array<std::uint32_t, ${word_count}> ${NAME}{${words}
};

} // namespace shaders
")
