#pragma once

#include "core/window.h"
#include "core/mesh.h"
#include "render_context.h"
#include "render_device.h"
#include "render_data.h"
#include "instanced_renderable.h"
#include "pipeline.h"

namespace Byte {

	class Renderer {
	private:
		RenderData _data;
		Pipeline _pipeline;
		
	public:
		Renderer() = default;

		void initialize(Window& window);

		void render(RenderContext& context);

		void load(Mesh& mesh);

		void load(InstancedRenderable& instanced);

		void load(Texture& instanced);

		void load(RenderContext& context);

		void update(Window& window);

		void resize(size_t width, size_t height);

		void submit(Shader&& shader);

		RenderDevice& device() {
			return _data.device;
		}

		const RenderDevice& device() const {
			return _data.device;
		}

		template<typename... Passes>
		static Renderer build() {
			Renderer renderer;

			renderer._pipeline = Pipeline::build<Passes...>();

			return renderer;
		}
	};

}
