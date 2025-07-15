#pragma once

#include <cstdint>
#include <initializer_list>

#include "core/core_types.h"
#include "material.h"
#include "render_types.h"

namespace Byte {

	struct Shader : public Asset {
		Path vertex;
		Path fragment;
		Path geometry;

		ShaderID id{};

		Set<Tag> uniforms;
	};

}