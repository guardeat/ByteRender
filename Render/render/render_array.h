#pragma once

#include <cstdint>

#include "render_buffer.h"

namespace Byte {

	struct RenderArray {
		RenderArrayID id;

		RenderBuffer vertexBuffer;
		RenderBuffer instanceBuffer;

		RenderBuffer indexBuffer;
	};

}