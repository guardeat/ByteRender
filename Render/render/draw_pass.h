#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "core/mesh.h"
#include "render_pass.h"
#include "render_context.h"
#include "render_data.h"
#include "render_device.h"
#include "shader.h"
#include "framebuffer.h"
#include "texture.h"
#include "camera.h"

namespace Byte {

	class DrawPass : public RenderPass<DrawPass> {
	private:
		AssetID _colorBuffer{};
		AssetID _geometryBuffer{};
		AssetID _quad{};
		AssetID _finalShader{};
		AssetID _fxaaShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };
			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };

			Mesh& quad{ data.meshes.at(_quad) };

			Shader* shader{};

			if (data.parameter<bool>("render_fxaa")) {
				shader = &data.shaders.at(_fxaaShader);
				data.device.shader().bind(*shader);

				float width{ static_cast<float>(data.width) };
				float height{ static_cast<float>(data.height) };
				data.device.shader().set(*shader, "uScreenSize", Vec2{ width,height });
			}
			else {
				shader = &data.shaders.at(_finalShader);
				data.device.shader().bind(*shader);
			}

			auto [camera, _] = context.camera();

			data.device.memory().bind(data.width, data.height);

			data.device.memory().bind(quad);

			data.device.memory().bind(colorBuffer.texture("color"), TextureUnit::UNIT_0);
			data.device.shader().set(*shader, "uColor", TextureUnit::UNIT_0);
			data.device.memory().bind(geometryBuffer.texture("depth"), TextureUnit::UNIT_1);
			data.device.shader().set(*shader, "uDepth", TextureUnit::UNIT_1);

			data.device.shader().set(*shader, "uGamma", data.parameter<float>("gamma"));
			data.device.shader().set(*shader, "uFar", camera.farPlane());
			data.device.shader().set(*shader, "uNear", camera.nearPlane());
			data.device.shader().set(*shader, "uFogColor", data.parameter<Vec3>("fog_color"));
			data.device.shader().set(*shader, "uFogNear", data.parameter<float>("fog_near"));
			data.device.shader().set(*shader, "uFogFar", data.parameter<float>("fog_far"));

			data.device.framebuffer().draw(quad.indexCount());
		}

		void initialize(RenderData& data) override {
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_geometryBuffer = data.parameter<AssetID>("geometry_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader finalShader{ shaderPath / "quad.vert",shaderPath / "final.frag" };
			_finalShader = finalShader.assetID();
			data.shaders.emplace(finalShader.assetID(), std::move(finalShader));

			Shader fxaaShader{ shaderPath / "quad.vert",shaderPath / "fxaa.frag" };
			_fxaaShader = fxaaShader.assetID();
			data.shaders.emplace(fxaaShader.assetID(), std::move(fxaaShader));

			data.parameter("gamma", 2.2f);
			data.parameter("fog_color", Vec3(0.5f, 0.5f, 0.5f));
			data.parameter("fog_near", 200.0f);
			data.parameter("fog_far", 300.0f);
			data.parameter("render_fxaa", true);
		}
	};

}
