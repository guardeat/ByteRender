#pragma once

#include <cstdint>

namespace Byte {

	using ShaderID = uint64_t;
	using TextureID = uint64_t;

	enum class DataType : uint32_t {
		BYTE = 0x1400,
		UNSIGNED_BYTE = 0x1401,
		SHORT = 0x1402,
		UNSIGNED_SHORT = 0x1403,
		INT = 0x1404,
		UNSIGNED_INT = 0x1405,
		FLOAT = 0x1406
	};

	enum class ColorFormat : uint32_t {
		DEPTH = 0x1902,
		RED = 0x1903,
		GREEN = 0x1904,
		BLUE = 0x1905,
		ALPHA = 0x1906,
		RGB = 0x1907,
		RGBA = 0x1908,
		RGBA32F = 0x8814,
		RGB32F = 0x8815,
		RGBA16F = 0x881A,
		RGB16F = 0x881B,
		R11F_G11F_B10F = 0x8C3A,
		R16F = 0x822D,
		R32F = 0x822E,
		R16 = 0x822A,
		RGB16 = 0x8054,
		RGBA16 = 0x805B
	};

	enum class AttachmentType : uint32_t {
		COLOR_0 = 0x8CE0,
		COLOR_1 = 0x8CE1,
		COLOR_2 = 0x8CE2,
		COLOR_3 = 0x8CE3,
		DEPTH = 0x8D00
	};

	enum class TextureFilter : uint32_t {
		NEAREST = 0x2600,
		LINEAR = 0x2601,
		NEAREST_MIPMAP_NEAREST = 0x2700,
		LINEAR_MIPMAP_NEAREST = 0x2701,
		NEAREST_MIPMAP_LINEAR = 0x2702,
		LINEAR_MIPMAP_LINEAR = 0x2703
	};

	enum class TextureWrap : uint32_t {
		REPEAT = 0x2901,
		MIRRORED_REPEAT = 0x8370,
		CLAMP_TO_EDGE = 0x812F,
		CLAMP_TO_BORDER = 0x812D
	};

}
