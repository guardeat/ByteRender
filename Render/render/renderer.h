#pragma once

#include "core/window.h"
#include "core/mesh.h"
#include "render_context.h"
#include "render_device.h"
#include "instanced_renderable.h"
#include "pipeline.h"

namespace Byte {

	struct RenderData {
		size_t width;
		size_t height;
	};

	class Renderer {
	private:
		RenderData _data;
		RenderDevice _device;
		Pipeline _pipeline;
		
	public:
		Renderer() = default;

		void initialize(Window& window);

		void render(RenderContext& context);

		void load(Mesh& mesh);

		void load(InstancedRenderable& instanced);

		void update(Window& window);

		void resize(size_t width, size_t height);
	};

}
