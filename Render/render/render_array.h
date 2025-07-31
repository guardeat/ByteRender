#pragma once

#include <cstdint>

#include <core/core_types.h>

namespace Byte {

	struct RenderArray {
		RenderArrayID id;

		Vector<RenderBufferID> renderBuffers;

		RenderBufferID indexBuffer;
	};

}