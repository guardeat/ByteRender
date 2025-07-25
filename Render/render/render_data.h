#pragma once

#include <variant>

#include "core/core_types.h"
#include "core/byte_math.h"
#include "render_device.h"
#include "shader.h"
#include "framebuffer.h"

namespace Byte {

	struct RenderData {
		size_t width{};
		size_t height{};

		Map<AssetID, Shader> shaders;
		Map<AssetID, Mesh> meshes;

		Map<Tag, Framebuffer> framebuffers;

		Map<Tag, Variant<bool, int, uint64_t, float, Vec3, Quaternion,Mat4>> parameters;

		RenderDevice device;

		template<typename Type>
		void parameter(const Tag& tag, Type&& value) {
			parameters[tag] = std::move(value);
		}

		template<typename Type>
		const Type& parameter(const Tag& tag) {
			return std::get<Type>(parameters.at(tag));
		}
	};

}
