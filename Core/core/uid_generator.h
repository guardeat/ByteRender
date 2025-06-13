#pragma once

#include <cstdint>
#include <random>

namespace Byte {

	struct UIDGenerator {
		inline static std::random_device rd;
		inline static std::mt19937_64 generator;
		inline static std::uniform_int_distribution<uint64_t> distribution;

		static uint64_t generate() {
			return distribution(generator);
		}
	};

}