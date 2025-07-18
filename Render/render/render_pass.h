#pragma once

#include "core/transform.h"
#include "render_context.h"
#include "render_data.h"

namespace Byte {

	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}
		
		virtual void terminate(RenderData& data) {
		}
	};

	class DrawPass: public RenderPass {
	public:
		void render(RenderData& data, RenderContext& context) override {
			for (auto [renderable, transform] : context.view<Renderable, Transform>()) {
				Mesh& mesh{ context.mesh(renderable.mesh()) };
				Material& material{ context.material(renderable.material()) };
				Shader& shader{ data.shaders.at(material.shader("default")) };

				data.device.bind(shader);
				data.device.bind(mesh);

				data.device.draw(mesh.indexCount());
			}
		}
	};

}