#pragma once

#include <cstdint>
#include <random>

namespace Byte {

    template<typename T>
    struct UIDGenerator;

    template<>
    struct UIDGenerator<uint32_t> {
        inline static std::random_device rd;
        inline static std::mt19937 generator{ rd() };
        inline static std::uniform_int_distribution<uint32_t> distribution;

        static uint32_t generate() {
            return distribution(generator);
        }
    };

    template<>
    struct UIDGenerator<uint64_t> {
        inline static std::random_device rd;
        inline static std::mt19937_64 generator{ rd() };
        inline static std::uniform_int_distribution<uint64_t> distribution;

        static uint64_t generate() {
            return distribution(generator);
        }
    };

}