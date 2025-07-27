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

	class SkyboxPass : public RenderPass {
	private:
		AssetID _skyboxShader{};
		AssetID _quad{};
		AssetID _skyboxMaterial{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& skyboxShader{ data.shaders.at(_skyboxShader) };
			Material& skyboxMaterial{ context.material(_skyboxMaterial) };

			auto [dLight, dLightTransform] = context.directionalLight();

			auto [camera, cameraTransform] = context.camera();

			Mat4 projection{ camera.perspective(16.0f / 9.0f) };
			Transform cTemp{ cameraTransform };
			cTemp.position(Vec3{});
			Mat4 view{ cTemp.view() };
			Mat4 inverseViewProjection{ (projection * view).inverse() };

			data.device.bind(skyboxShader);
			data.device.bind(quad);

			data.device.uniform(skyboxShader, skyboxMaterial);
			data.device.uniform(skyboxShader, "uDLight.direction", dLightTransform.front());
			data.device.uniform(skyboxShader, "uDLight.color", dLight.color);
			data.device.uniform(skyboxShader, "uDLight.intensity", dLight.intensity);
			data.device.uniform(skyboxShader, "uInverseViewProjection", inverseViewProjection);

			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.draw(quad.indexCount());
			data.device.state(RenderState::ENABLE_DEPTH);
		}

		UniquePtr<RenderPass> clone() const override {
			return std::make_unique<SkyboxPass>();
		}

		void initialize(RenderData& data) {
			Shader skyboxShader{ "../Render/shader/skybox.vert","../Render/shader/skybox.frag" };
			skyboxShader.uniforms().insert("uScatter");
			_skyboxShader = skyboxShader.assetID();
			data.shaders.emplace(skyboxShader.assetID(), std::move(skyboxShader));

			Mesh quad{ Primitive::quad() };
			_quad = quad.assetID();
			data.meshes.emplace(quad.assetID(), std::move(quad));

			_skyboxMaterial = data.parameter<AssetID>("skybox_material");
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

		UniquePtr<RenderPass> clone() const override {
			return std::make_unique<DrawPass>();
		}
	};

}