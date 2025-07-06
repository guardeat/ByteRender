#pragma once

#include <cstdint>

#include <core/core_types.h>
#include "render_buffer.h"

namespace Byte {

	struct RenderArray {
		RenderArrayID id;

		Vector<RenderBuffer> renderBuffers;

		RenderBuffer indexBuffer;
	};

}