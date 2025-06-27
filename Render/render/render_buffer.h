#pragma once

#include"core/layout.h"
#include "render_types.h"

namespace Byte {

	struct RenderBuffer{
		RenderBufferID id;

		Layout layout;

		BufferMode mode;
	};

}
