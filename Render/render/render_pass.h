#pragma once

#include "core/core_types.h"
#include "core/transform.h"
#include "render_context.h"
#include "render_data.h"
#include "camera.h"

namespace Byte {

	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual UniquePtr<RenderPass> clone() const = 0;

		virtual void initialize(RenderData& data) {
		}
		
		virtual void terminate(RenderData& data) {
		}

	};

	class DrawPass: public RenderPass {
	public:
		void render(RenderData& data, RenderContext& context) override {
			auto [camera, cameraTransform] = context.camera();
			auto [dLight, dLightTransform] = context.directionalLight();

			Mat4 projection{ camera.perspective(16.0f / 9.0f) };
			Mat4 view{ cameraTransform.view() };

			for (auto [renderable, transform] : context.view<Renderable, Transform>()) {
				Mesh& mesh{ context.mesh(renderable.mesh()) };
				Material& material{ context.material(renderable.material()) };
				Shader& shader{ data.shaders.at(material.shader("default")) };

				data.device.bind(shader);
				data.device.bind(mesh);

				data.device.uniform(shader, transform);
				data.device.uniform(shader, "uProjection", projection);
				data.device.uniform(shader, "uView", view);
				data.device.uniform(shader, material);
				data.device.uniform(shader, "uDLight.direction", dLightTransform.front());
				data.device.uniform(shader, "uDLight.color", dLight.color);
				data.device.uniform(shader, "uDLight.intensity", dLight.intensity);

				data.device.draw(mesh.indexCount());
			}
		}

		virtual UniquePtr<RenderPass> clone() const {
			return std::make_unique<DrawPass>();
		}
	};

}