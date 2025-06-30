#pragma once

#include "render_context.h"
#include "render_data.h"
#include "render_device.h"
#include "core/window.h"
#include "pipeline.h"

namespace Byte {

	class Renderer {
	private:
		RenderData _data;
		RenderDevice _device;
		Pipeline _pipeline;
		
	public:
		Renderer() = default;

		void initialize(Window& window);

		void render(RenderContext& context);

	};

}
