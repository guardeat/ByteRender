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

namespace Byte {

	class BloomPass : public RenderPass<BloomPass> {
	private:
		AssetID _colorBuffer{};
		AssetID _bloomUpShader{};
		AssetID _bloomDownShader{};
		AssetID _quad{};
		Vector<AssetID> _bloomBuffers{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			if (!data.parameter<bool>("render_bloom")) {
				return;
			}

			size_t mipCount{ data.parameter<size_t>("bloom_mipmap_levels") };

			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };
			Framebuffer& bloomBuffer{ data.framebuffers.at(*_bloomBuffers.begin()) };

			data.device.memory().bind(bloomBuffer);
			data.device.clearBuffer();

			Mesh& quad{ data.meshes.at(_quad) };
			Shader& downShader{ data.shaders.at(_bloomDownShader) };

			float gamma{ data.parameter<float>("gamma") };
			float width{ static_cast<float>(colorBuffer.width()) };
			float height{ static_cast<float>(colorBuffer.height()) };

			data.device.memory().bind(downShader);
			data.device.uniform(downShader, "uInverseGamma", 1.0f / gamma);
			data.device.uniform(downShader, "uKarisAverage", true);
			data.device.uniform(downShader, "uSrcResolution", Vec2{ width,height });
			data.device.uniform(downShader, "uSrcTexture", colorBuffer.texture("color"));

			data.device.memory().bind(quad);
			data.device.draw(quad.indexCount());

			for (size_t i{ 1 }; i < mipCount; ++i) {
				Texture& srcTexture{ bloomBuffer.texture("bloom") };

				width = static_cast<float>(bloomBuffer.width());
				height = static_cast<float>(bloomBuffer.height());

				Framebuffer& bloomBuffer{ data.framebuffers.at(_bloomBuffers.at(i)) };

				data.device.memory().bind(bloomBuffer);
				data.device.clearBuffer();

				data.device.uniform(downShader, "uSrcTexture", srcTexture);
				data.device.uniform(downShader, "uSrcResolution", Vec2{ width, height });

				data.device.draw(quad.indexCount());

				data.device.uniform(downShader, "uKarisAverage", false);
			}

			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.state(RenderState::ENABLE_BLEND);

			Shader& upShader{ data.shaders.at(_bloomUpShader) };
			data.device.memory().bind(upShader);
			data.device.uniform(upShader, "uFilterRadius", 0.005f);

			for (size_t i{ mipCount - 1 }; i > 0; --i) {
				Framebuffer& sourceBuffer{ data.framebuffers.at(_bloomBuffers.at(i)) };
				Texture& srcTexture{ sourceBuffer.texture("bloom") };

				Framebuffer& bloomBuffer{ data.framebuffers.at(_bloomBuffers.at(i - 1)) };

				data.device.memory().bind(bloomBuffer);
				data.device.clearBuffer();

				data.device.uniform(upShader, "uSrcTexture", srcTexture);

				data.device.draw(quad.indexCount());
			}

			float strength{ data.parameter<float>("bloom_strength") };

			data.device.blendWeights(strength, 1 - strength);
			data.device.state(RenderState::BLEND_WEIGHTED);

			data.device.memory().bind(colorBuffer);
			Texture& srcTexture{ bloomBuffer.texture("bloom") };
			data.device.uniform(upShader, "uSrcTexture", srcTexture);

			data.device.draw(quad.indexCount());

			data.device.state(RenderState::DISABLE_BLEND);
			data.device.state(RenderState::ENABLE_DEPTH);
			data.device.state(RenderState::BLEND_ADD);
		}

		void initialize(RenderData& data) override {
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Framebuffer bloomFramebuffer{ data.width, data.height };

			constexpr size_t MIPMAP_LEVELS{ 3 };

			data.parameter("bloom_mipmap_levels", MIPMAP_LEVELS);

			float divisor{ 2.0f };
			for (size_t idx{}; idx < MIPMAP_LEVELS; ++idx) {
				Texture bloomTexture;
				bloomTexture.attachment(AttachmentType::COLOR_0);
				bloomTexture.internalFormat(ColorFormat::R11F_G11F_B10F);
				bloomTexture.format(ColorFormat::RGB);
				bloomTexture.dataType(DataType::FLOAT);

				bloomTexture.attachment(AttachmentType::COLOR_0);

				Framebuffer bloomFramebuffer{
					static_cast<size_t>(static_cast<float>(data.width) / divisor),
					static_cast<size_t>(static_cast<float>(data.height) / divisor)
				};

				bloomFramebuffer.resizeFactor(1.0f / divisor);

				bloomFramebuffer.texture(Tag{ "bloom" }, std::move(bloomTexture));

				divisor *= 2.0f;

				_bloomBuffers.push_back(bloomFramebuffer.assetID());
				data.parameter(Tag{ "bloom_framebuffer_id_" } + std::to_string(idx), bloomFramebuffer.assetID());
				data.framebuffers.emplace(bloomFramebuffer.assetID(), std::move(bloomFramebuffer));
			}

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader bloomUpShader{ shaderPath / "quad.vert", shaderPath / "bloom_upsample.frag" };
			_bloomUpShader = bloomUpShader.assetID();
			data.shaders.emplace(bloomUpShader.assetID(), std::move(bloomUpShader));

			Shader bloomDownShader{ shaderPath / "quad.vert", shaderPath / "bloom_downsample.frag" };
			_bloomDownShader = bloomDownShader.assetID();
			data.shaders.emplace(bloomDownShader.assetID(), std::move(bloomDownShader));

			data.parameter("bloom_down_shader_id", _bloomDownShader);
			data.parameter("bloom_up_shader_id", _bloomUpShader);
			data.parameter("render_bloom", true);
			data.parameter("bloom_strength", 0.3f);
		}
	};

}
