#pragma once

#include "core/core_types.h"
#include "core/transform.h"
#include "render_context.h"
#include "render_data.h"
#include "camera.h"
#include "mesh_renderer.h"

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
		AssetID _colorBuffer{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& skyboxShader{ data.shaders.at(_skyboxShader) };
			Material& skyboxMaterial{ context.material(_skyboxMaterial) };

			auto [dLight, dLightTransform] = context.directionalLight();

			auto [camera, cameraTransform] = context.camera();

			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };
			Mat4 projection{ camera.perspective(aspect) };
			Transform cTemp{ cameraTransform };
			cTemp.position(Vec3{});
			Mat4 view{ cTemp.view() };
			Mat4 inverseViewProjection{ (projection * view).inverse() };
			
			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };
			data.device.bind(colorBuffer);
			data.device.clearBuffer();

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

			data.device.bindDefault(data.width, data.height);
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
			data.parameter<AssetID>("quad_mesh_id", quad.assetID());
			data.meshes.emplace(quad.assetID(), std::move(quad));

			_skyboxMaterial = data.parameter<AssetID>("skybox_material");

			Framebuffer colorBuffer{ data.width, data.height };
			data.parameter<AssetID>("color_buffer_id", colorBuffer.assetID());
			_colorBuffer = colorBuffer.assetID();

			Texture colorTexture{};
			colorTexture.attachment(AttachmentType::COLOR_0);
			colorTexture.internalFormat(ColorFormat::R11F_G11F_B10F);
			colorTexture.format(ColorFormat::RGB);
			colorTexture.dataType(DataType::FLOAT);

			colorBuffer.texture(Tag{ "color" }, std::move(colorTexture));

			data.framebuffers.emplace(colorBuffer.assetID(), std::move(colorBuffer));
		}
	};

	class DrawPass: public RenderPass {
	private:
		AssetID _colorBuffer{};
		AssetID _quad{};
		AssetID _finalShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			auto [camera, cameraTransform] = context.camera();
			auto [dLight, dLightTransform] = context.directionalLight();

			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };
			Mat4 projection{ camera.perspective(aspect) };
			Mat4 view{ cameraTransform.view() };

			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };
			data.device.bind(colorBuffer);

			for (auto [renderable, transform] : context.view<MeshRenderer, Transform>()) {
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
			
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& finalShader{ data.shaders.at(_finalShader) };

			data.device.bind(quad);
			data.device.bind(finalShader);

			data.device.bindDefault(data.width, data.height);
			
			data.device.uniform(finalShader, "uAlbedo", colorBuffer.texture("color"));
			data.device.uniform(finalShader, "uGamma", data.parameter<float>("gamma"));
			data.device.draw(quad.indexCount());
		}

		UniquePtr<RenderPass> clone() const override {
			return std::make_unique<DrawPass>();
		}

		void initialize(RenderData& data) override {
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Shader finalShader{ "../Render/shader/final.vert","../Render/shader/final.frag" };
			_finalShader = finalShader.assetID();
			data.shaders.emplace(finalShader.assetID(), std::move(finalShader));

			data.parameter("gamma", 2.2f);
		}
	};

}