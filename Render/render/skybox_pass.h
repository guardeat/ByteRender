#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "core/transform.h"
#include "core/mesh.h"
#include "render_pass.h"
#include "render_context.h"
#include "render_data.h"
#include "render_device.h"
#include "shader.h"
#include "framebuffer.h"
#include "texture.h"
#include "camera.h"
#include "light.h"

namespace Byte {

	class SkyboxPass : public RenderPass<SkyboxPass> {
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

			data.device.uniform(skyboxShader, skyboxMaterial, context.repository());
			data.device.uniform(skyboxShader, "uDLight.direction", dLightTransform.front());
			data.device.uniform(skyboxShader, "uDLight.color", dLight.color);
			data.device.uniform(skyboxShader, "uDLight.intensity", dLight.intensity);
			data.device.uniform(skyboxShader, "uInverseViewProjection", inverseViewProjection);

			data.device.renderState(RenderState::DISABLE_DEPTH);
			data.device.draw(quad.indexCount());
			data.device.renderState(RenderState::ENABLE_DEPTH);
		}

		void initialize(RenderData& data) {
			Path shaderPath{ data.parameter<Path>("default_shader_path") };
			Shader skyboxShader{ shaderPath / "skybox.vert",shaderPath / "skybox.frag" };
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

}
