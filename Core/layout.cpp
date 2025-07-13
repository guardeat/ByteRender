#include "layout.h"

namespace Byte {
	
	Layout::Layout(const InitializerList<uint64_t>& values)
		:_data{ std::make_unique<uint8_t[]>(values.size())} {
		size_t index{ 0 };
		for (uint64_t value : values) {
			_data[index++] = static_cast<uint8_t>(value);
			_stride += value;
		}

		_size = values.size();
	}

	Layout::Layout(const Layout& layout)
		: _size{ layout._size }, _stride{ layout._stride }, _data{ std::make_unique<uint8_t[]>(_size) } {
		for (size_t index{}; index < _size; ++index) {
			_data[index] = layout._data[index];
		}
	}

	Layout& Layout::operator=(const Layout& layout) {
		_data.reset();
		if (this != &layout) {
			_size = layout._size;
			_stride = layout._stride;
			_data = std::make_unique<uint8_t[]>(_size);
			for (size_t index{}; index < _size; ++index) {
				_data[index] = layout._data[index];
			}
		}
		return *this;
	}


	size_t Layout::stride() const {
		return _stride;
	}

	uint8_t Layout::operator[](size_t index) const {
		return _data[index];
	}

	size_t Layout::size() const {
		return _size;
	}

	uint8_t* Layout::data() {
		return _data.get();
	}

	const uint8_t* Layout::data() const {
		return _data.get();
	}

}
