#pragma once

#include <cstdint>

#include <core/core_types.h>

namespace Byte {

	struct RenderBuffer {
		RenderBufferID id;

		Layout layout;

		BufferMode mode;
	};

	struct RenderArray {
		RenderArrayID id;

		Vector<RenderBuffer> renderBuffers;

		RenderBuffer indexBuffer;
	};

}