#pragma once

#include <chrono>

namespace Byte {

	class Timer {
	private:
		using TimePoint = std::chrono::high_resolution_clock::time_point;

		TimePoint _start;
	public:
		Timer() {
			reset();
		}

		float elapsed() const {
			auto now{ std::chrono::high_resolution_clock::now() };
			return std::chrono::duration<float>(now - _start).count();
		}

		void reset() {
			_start = std::chrono::high_resolution_clock::now();
		}
	};

}