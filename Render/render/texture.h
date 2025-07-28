#pragma once

#include "render_types.h"

namespace Byte {

	class Texture: public Asset {
	private:
		ColorFormat _internalFormat{ ColorFormat::RGBA };
		ColorFormat _format{ ColorFormat::RGBA };
		DataType _dataType{ DataType::UNSIGNED_BYTE };

		size_t _width{};
		size_t _height{};

		TextureWrap _wrapS{ TextureWrap::CLAMP_TO_EDGE };
		TextureWrap _wrapT{ TextureWrap::CLAMP_TO_EDGE };
		TextureFilter _minFilter{ TextureFilter::LINEAR };
		TextureFilter _magFilter{ TextureFilter::LINEAR };

		Vector<uint8_t> _data{};

		Path _path{};

	public:
		Texture() = default;

		ColorFormat internalFormat() const { 
			return _internalFormat; 
		}

		void internalFormat(ColorFormat value) {
			_internalFormat = value; 
		}

		ColorFormat format() const {
			return _format; 
		}

		void format(ColorFormat value) { 
			_format = value; 
		}

		DataType dataType() const { 
			return _dataType; 
		}

		void dataType(DataType value) { 
			_dataType = value; 
		}

		size_t width() const { 
			return _width; 
		}
		void width(size_t value) {
			_width = value; 
		}

		size_t height() const { 
			return _height; 
		}

		void height(size_t value) { 
			_height = value;
		}

		TextureWrap wrapS() const { 
			return _wrapS; 
		}

		void wrapS(TextureWrap value) { 
			_wrapS = value; 
		}

		TextureWrap wrapT() const { 
			return _wrapT; 
		}

		void wrapT(TextureWrap value) { 
			_wrapT = value; 
		}

		TextureFilter minFilter() const { 
			return _minFilter; 
		}

		void minFilter(TextureFilter value) { 
			_minFilter = value; 
		}

		TextureFilter magFilter() const { 
			return _magFilter; 
		}

		void magFilter(TextureFilter value) { 
			_magFilter = value; 
		}

		const Vector<uint8_t>& data() const {
			return _data; 
		}

		void data(const Vector<uint8_t>& value) {
			_data = value;
		}

		const Path& path() const { 
			return _path;
		}

		void path(const Path& value) { 
			_path = value; 
		}
	};

}
