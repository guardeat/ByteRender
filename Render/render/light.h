#pragma once

#include "core/byte_math.h"

namespace Byte {

	struct DirectionalLight {
		Vec3 color{ 1.0f,1.0f,1.0f };

		float intensity{ 1.0f };
	};

}