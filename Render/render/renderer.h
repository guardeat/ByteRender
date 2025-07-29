#pragma once

#include "core/window.h"
#include "core/mesh.h"
#include "render_context.h"
#include "render_device.h"
#include "render_data.h"
#include "pipeline.h"

namespace Byte {

	class Renderer {
	private:
		RenderData _data;
		Pipeline _pipeline;
		
	public:
		Renderer() = default;

		~Renderer();

		void initialize(Window& window);

		void render(RenderContext& context);

		void load(RenderContext& context);

		void release(Mesh& mesh);

		void release(InstanceGroup& group);

		void release(Shader& shader);

		void release(Texture& texture);

		void update(Window& window);

		void resize(size_t width, size_t height);

		void submit(Shader&& shader);

		void clearMemory();

		template<typename Type>
		void parameter(const Tag& tag, Type&& value) {
			_data.parameter(tag, std::move(value));
		}

		template<typename Type>
		const Type& parameter(const Tag& tag) {
			return _data.parameter(tag);
		}

		template<typename... Passes>
		static Renderer build() {
			Renderer renderer;

			renderer._pipeline = Pipeline::build<Passes...>();

			return renderer;
		}
	};

}
