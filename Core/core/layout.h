#pragma once

#include <memory>
#include <initializer_list>
#include <stdexcept> 

#include "core_types.h"

namespace Byte {

	class Layout {
	private:
		size_t _size;
		size_t _stride{};
		std::unique_ptr<uint8_t[]> _data;

	public:
		Layout(const InitializerList<uint64_t>& values);

		Layout(const Layout& layout);

		Layout& operator=(const Layout& layout);

		Layout(Layout&& layout) noexcept = default;

		Layout& operator=(Layout&& layour) noexcept = default;

		size_t stride() const;

		uint8_t operator[](size_t _index) const;

		size_t size() const;

		uint8_t* data();

		const uint8_t* data() const;
	};

}
