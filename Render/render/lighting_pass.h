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

	class LightingPass : public RenderPass<LightingPass> {
	private:
		AssetID _geometryBuffer{};
		AssetID _colorBuffer{};
		AssetID _lightingShader{};
		AssetID _pointLightShader{};
		AssetID _quad{};
		AssetID _pointLightGroup{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			bindBuffersAndShaders(data);
			setDirectionalLightUniforms(data, context);
			drawDirectionalLight(data);
			drawPointLights(data, context);
		}

		void initialize(RenderData& data) override {
			_geometryBuffer = data.parameter<AssetID>("geometry_buffer_id");
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader lightingShader{ shaderPath / "quad.vert", shaderPath / "lighting.frag" };
			_lightingShader = lightingShader.assetID();
			data.parameter("lighting_shader_id", lightingShader.assetID());
			data.shaders.emplace(lightingShader.assetID(), std::move(lightingShader));

			Shader pointLightShader{ shaderPath / "point_light.vert", shaderPath / "point_light.frag" };
			_pointLightShader = pointLightShader.assetID();
			data.parameter("point_light_shader_id", pointLightShader.assetID());
			data.shaders.emplace(pointLightShader.assetID(), std::move(pointLightShader));

			_pointLightGroup = data.parameter<AssetID>("point_light_group_id");
		}

	private:
		void bindBuffersAndShaders(RenderData& data) {
			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };
			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };

			data.device.framebuffer().bind(colorBuffer);

			Mesh& quad{ data.meshes.at(_quad) };
			Shader& lightingShader{ data.shaders.at(_lightingShader) };

			data.device.memory().bind(quad);
			data.device.shader().bind(lightingShader);

			data.device.memory().bind(geometryBuffer.texture("normal"), TextureUnit::UNIT_0);
			data.device.shader().set(lightingShader, "uNormal", TextureUnit::UNIT_0);
			data.device.memory().bind(geometryBuffer.texture("albedo"), TextureUnit::UNIT_1);
			data.device.shader().set(lightingShader, "uAlbedo", TextureUnit::UNIT_1);
			data.device.memory().bind(geometryBuffer.texture("material"), TextureUnit::UNIT_2);
			data.device.shader().set(lightingShader, "uMaterial", TextureUnit::UNIT_2);
			data.device.memory().bind(geometryBuffer.texture("depth"), TextureUnit::UNIT_3);
			data.device.shader().set(lightingShader, "uDepth", TextureUnit::UNIT_3);
		}

		void setDirectionalLightUniforms(RenderData& data, RenderContext& context) {
			auto [directionalLight, dLightTransform] = context.directionalLight();
			auto [camera, cameraTransform] = context.camera();
			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };

			Mat4 view{ cameraTransform.view() };
			Mat4 inverseView{ view.inverse() };
			Mat4 projection{ camera.perspective(aspect) };
			Mat4 inverseProjection{ projection.inverse() };

			Shader& lightingShader{ data.shaders.at(_lightingShader) };

			data.device.shader().set(lightingShader, "uDLight.direction", dLightTransform.front());
			data.device.shader().set(lightingShader, "uDLight.color", directionalLight.color);
			data.device.shader().set(lightingShader, "uDLight.intensity", directionalLight.intensity);
			
			data.device.shader().set(lightingShader, "uView", view);
			data.device.shader().set(lightingShader, "uInverseView", inverseView);
			data.device.shader().set(lightingShader, "uInverseProjection", inverseProjection);
			data.device.shader().set(lightingShader, "uViewPos", cameraTransform.position());

			size_t cascadeCount{ data.parameter<uint64_t>("cascade_count") };
			data.device.shader().set(lightingShader, "uCascadeCount", cascadeCount);

			for (size_t idx{}; idx < cascadeCount; ++idx) {
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };
				float cascadeDivisor{ data.parameter<float>("cascade_divisor_" + std::to_string(idx)) };
				float cascadeFar{ camera.farPlane() / cascadeDivisor };
				AssetID shadowBufferID{ data.parameter<AssetID>("shadow_buffer_id_" + std::to_string(idx)) };
				Texture& depthTexture{ data.framebuffers.at(shadowBufferID).texture("depth") };
				TextureUnit unit{ static_cast<TextureUnit>(static_cast<size_t>(TextureUnit::UNIT_4) + idx) };

				data.device.shader().set(lightingShader, "uLightSpaces[" + std::to_string(idx) + "]", lightSpace);
				data.device.shader().set(lightingShader, "uCascadeFars[" + std::to_string(idx) + "]", cascadeFar);
				data.device.memory().bind(depthTexture, unit);
				data.device.shader().set(lightingShader, "uDepthMaps[" + std::to_string(idx) + "]", unit);
			}
		}

		void drawDirectionalLight(RenderData& data) {
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& lightingShader{ data.shaders.at(_lightingShader) };
			data.device.memory().bind(quad);
			data.device.shader().bind(lightingShader);
			data.device.framebuffer().draw(quad.indexCount());
		}

		void drawPointLights(RenderData& data, RenderContext& context) {
			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.state(RenderState::ENABLE_BLEND);
			data.device.state(RenderState::BLEND_ADD);
			data.device.state(RenderState::ENABLE_CULLING);
			data.device.state(RenderState::CULL_FRONT);

			InstanceGroup& pointLightGroup{ context.instanceGroup(_pointLightGroup) };
			Mesh& pointLightMesh{ context.mesh(pointLightGroup.mesh()) };
			Shader& pointLightShader{ data.shaders.at(_pointLightShader) };

			auto [camera, cameraTransform] = context.camera();
			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };
			Vec2 viewPortSize{ static_cast<float>(data.width), static_cast<float>(data.height) };

			data.device.shader().bind(pointLightShader);
			data.device.memory().bind(pointLightGroup);

			data.device.shader().set(pointLightShader, "uProjection", camera.perspective(aspect));
			data.device.shader().set(pointLightShader, "uView", cameraTransform.view());
			data.device.shader().set(pointLightShader, "uInverseView", cameraTransform.view().inverse());
			data.device.shader().set(pointLightShader, "uInverseProjection", camera.perspective(aspect).inverse());
			data.device.shader().set(pointLightShader, "uViewPos", cameraTransform.position());
			data.device.shader().set(pointLightShader, "uViewPortSize", viewPortSize);

			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };
			data.device.memory().bind(geometryBuffer.texture("normal"), TextureUnit::UNIT_0);
			data.device.shader().set(pointLightShader, "uNormal", TextureUnit::UNIT_0);
			data.device.memory().bind(geometryBuffer.texture("albedo"), TextureUnit::UNIT_1);
			data.device.shader().set(pointLightShader, "uAlbedo", TextureUnit::UNIT_1);
			data.device.memory().bind(geometryBuffer.texture("material"), TextureUnit::UNIT_2);
			data.device.shader().set(pointLightShader, "uMaterial", TextureUnit::UNIT_2);
			data.device.memory().bind(geometryBuffer.texture("depth"), TextureUnit::UNIT_3);
			data.device.shader().set(pointLightShader, "uDepth", TextureUnit::UNIT_3);

			data.device.framebuffer().draw(pointLightMesh.indexCount(), pointLightGroup.count(), DrawType::TRIANGLES);

			data.device.state(RenderState::ENABLE_DEPTH);
			data.device.state(RenderState::DISABLE_BLEND);
			data.device.state(RenderState::CULL_BACK);
			data.device.state(RenderState::DISABLE_CULLING);
		}
	};

}
