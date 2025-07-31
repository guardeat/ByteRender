#pragma once

#include <cstdint>
#include <functional>

#include "core/core_types.h"
#include "ecs/ecs.h"

namespace Byte {

	using RenderID = EntityID;

	using GPUResourceID = uint32_t;

	struct GPUResource {
		GPUResourceID id{};

		GPUResource(GPUResourceID id = 0)
			:id{ id } {
		}

		operator GPUResourceID() const {
			return id;
		}
	};

	struct GPUBuffer : public GPUResource {
		using GPUResource::GPUResource;
	};

	struct GPUBufferGroup : public GPUResource {
		using GPUResource::GPUResource;

		Vector<GPUBuffer> renderBuffers;
		GPUBuffer indexBuffer;
	};

	struct GPUShader : public GPUResource {
		using GPUResource::GPUResource;
	};

	struct GPUTexture : public GPUResource {
		using GPUResource::GPUResource;
	};

	struct GPUFramebuffer : public GPUResource {
		using GPUResource::GPUResource;
	};

	enum class TransparencyMode : uint8_t {
		OPAQUE,
		BINARI,
		GRADIENT,
	};

	enum class DataType : uint8_t {
		BYTE,
		UNSIGNED_BYTE,
		SHORT,
		UNSIGNED_SHORT,
		INT,
		UNSIGNED_INT,
		FLOAT
	};

	enum class ColorFormat : uint8_t {
		DEPTH,
		RED,
		GREEN,
		BLUE,
		ALPHA,
		RGB,
		RGBA,
		RGBA32F,
		RGB32F,
		RGBA16F,
		RGB16F,
		R11F_G11F_B10F,
		R16F,
		R32F,
		R16,
		RGB16,
		RGBA16
	};

	enum class AttachmentType : uint8_t {
		COLOR_0,
		COLOR_1,
		COLOR_2,
		COLOR_3,
		DEPTH
	};

	enum class TextureUnit : uint8_t {
		UNIT_0,
		UNIT_1,
		UNIT_2,
		UNIT_3,
		UNIT_4,
		UNIT_5,
		UNIT_6,
		UNIT_7
	};

	enum class TextureFilter : uint8_t {
		NEAREST,
		LINEAR,
		NEAREST_MIPMAP_NEAREST,
		LINEAR_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_LINEAR
	};

	enum class TextureWrap : uint8_t {
		REPEAT,
		MIRRORED_REPEAT,
		CLAMP_TO_EDGE,
		CLAMP_TO_BORDER
	};

	enum class BufferMode : uint8_t {
		STATIC,
		DYNAMIC,
	};

	enum class ShaderType : uint8_t {
		VERTEX,
		FRAGMENT,
		GEOMETRY,
	};

	enum class DrawType : uint8_t {
		POINTS,
		LINES,
		LINE_LOOP,
		LINE_STRIP,
		TRIANGLES,
		TRIANGLE_STRIP,
		TRIANGLE_FAN
	};

	enum class RenderState : uint8_t {
		ENABLE_DEPTH,
		DISABLE_DEPTH,
		ENABLE_BLEND,
		DISABLE_BLEND,
		ENABLE_CULLING,
		DISABLE_CULLING,
		CULL_BACK,
		CULL_FRONT
	};

}
