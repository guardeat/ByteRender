#pragma once

#include <variant>

#include "core/core_types.h"
#include "render_device.h"

namespace Byte {

	struct RenderData {
		size_t width{};
		size_t height{};

		Map<Tag, Variant<bool, int, uint64_t, float, Vec3, Quaternion>> settings;

		RenderDevice device;

		template<typename Type>
		void setting(const Tag& tag, const Type& value) {
			settings[tag] = value;
		}

		template<typename Type>
		void setting(const Tag& tag) {
			return std::get<Type>(settings.at(tag));
		}
	};

}
