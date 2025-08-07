#pragma once

#include <limits>

#include "core/core_types.h"
#include "core/asset.h"
#include "core/transform.h"
#include "core/mesh.h"
#include "render_types.h"
#include "render_pass.h"
#include "render_context.h"
#include "render_data.h"
#include "mesh_renderer.h"
#include "framebuffer.h"
#include "camera.h"
#include "render_device.h"
#include "texture.h"
#include "shader.h"

namespace Byte {

	class ShadowPass : public RenderPass<ShadowPass> {
	private:
		Vector<AssetID> _shadowBuffers{};
		AssetID _shadowShader{};
		AssetID _instancedShadowShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			if (!data.parameter<bool>("render_shadow")) {
				return;
			}

			auto [dLight, dLightTransform] = context.directionalLight();
			auto [camera, cameraTransform] = context.camera();

			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };

			float far{ camera.farPlane() };
			float near{ camera.nearPlane() };
			Mat4 cameraView{ cameraTransform.view() };

			for (size_t idx{}; idx < data.parameter<uint64_t>("cascade_count"); ++idx) {
				float cascadeDivisor{ data.parameter<float>("cascade_divisor_" + std::to_string(idx)) };
				Mat4 projection{ camera.perspective(aspect, near, far / cascadeDivisor) };
				Mat4 lightSpace{ frustumSpace(projection, cameraView, dLightTransform, far) };
				data.parameter("light_space_matrix_" + std::to_string(idx), lightSpace);
			}

			for (size_t idx{}; idx < data.parameter<uint64_t>("cascade_count"); ++idx) {
				Framebuffer& shadowBuffer{ data.framebuffers.at(_shadowBuffers[idx]) };
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };

				data.device.bind(shadowBuffer);
				data.device.clearBuffer();

				Shader& shadowShader{ data.shaders.at(_shadowShader) };
				data.device.bind(shadowShader);
				data.device.uniform(shadowShader, "uLightSpace", lightSpace);
				for (auto [renderer, transform] : context.view<MeshRenderer, Transform>()) {
					if (renderer.mesh() == 0 || renderer.material() == 0 || !renderer.shadow()) {
						continue;
					}
					Mesh& mesh{ context.mesh(renderer.mesh()) };
					data.device.bind(mesh);

					data.device.uniform(shadowShader, transform);

					data.device.draw(mesh.indexCount());
				}

				Shader& instancedShadowShader{ data.shaders.at(_instancedShadowShader) };
				data.device.bind(instancedShadowShader);
				data.device.uniform(instancedShadowShader, "uLightSpace", lightSpace);

				for (auto& [_, group] : context.instanceGroups()) {
					if (group.mesh() == 0 || group.count() == 0 || !group.shadow()) {
						continue;
					}
					Mesh& mesh{ context.mesh(group.mesh()) };
					data.device.bind(group);

					data.device.draw(mesh.indexCount(), group.count());
				}
			}
		}

		void initialize(RenderData& data) override {
			Path shaderPath{ data.parameter<Path>("default_shader_path") };
			Shader shadowShader{ shaderPath / "depth.vert", shaderPath / "depth.frag" };
			Shader instancedShadowShader{ shaderPath / "instanced_depth.vert", shaderPath / "depth.frag" };

			_shadowShader = shadowShader.assetID();
			_instancedShadowShader = instancedShadowShader.assetID();

			data.shaders.emplace(shadowShader.assetID(), std::move(shadowShader));
			data.shaders.emplace(instancedShadowShader.assetID(), std::move(instancedShadowShader));

			constexpr size_t CASCADE_COUNT{ 4 };
			data.parameter("cascade_count", CASCADE_COUNT);

			constexpr size_t SHADOW_BUFFER_SIZE{ 2048 };
			data.parameter("shadow_buffer_size", SHADOW_BUFFER_SIZE);

			for (size_t idx{}; idx < CASCADE_COUNT; ++idx) {
				float x{ static_cast<float>(idx) };
				float divisor{ 0.833f * x * x * x - 0.25f * x * x + 0.417f * x + 1.0f };
				data.parameter("cascade_divisor_" + std::to_string(idx), divisor);
				data.parameter("light_space_matrix_" + std::to_string(idx), Mat4{});
				Framebuffer buffer{ SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE };
				buffer.resize(false);

				Texture depthTexture{};
				depthTexture.attachment(AttachmentType::DEPTH);
				depthTexture.internalFormat(ColorFormat::DEPTH32F);
				depthTexture.format(ColorFormat::DEPTH);
				depthTexture.dataType(DataType::FLOAT);

				buffer.texture(Tag{ "depth" }, std::move(depthTexture));

				_shadowBuffers.push_back(buffer.assetID());
				data.parameter(Tag{ "shadow_buffer_id_" } + std::to_string(idx), buffer.assetID());
				data.framebuffers.emplace(buffer.assetID(), buffer);
			}

			data.parameter("render_shadow", true);
		}

	private:
		Mat4 frustumSpace(
			const Mat4& projection,
			const Mat4& view,
			const Transform& lightTransform,
			float far) const {
			Mat4 inv{ (projection * view).inverse() };

			Vector<Vec4> corners;
			for (size_t x{}; x < 2; ++x) {
				for (size_t y{}; y < 2; ++y) {
					for (size_t z{}; z < 2; ++z) {
						Vec4 pt{
							inv * Vec4{
								2.0f * x - 1.0f,
								2.0f * y - 1.0f,
								2.0f * z - 1.0f,
								1.0f} };
						corners.push_back(pt / pt.w);
					}
				}
			}

			Vec3 center{};
			for (Vec4& v : corners) {
				center += Vec3{ v.x, v.y, v.z };
			}
			center /= corners.size();

			Mat4 lightView{ Mat4::view(
				center - lightTransform.front(),
				center,
				lightTransform.up()
			) };

			float minX{ std::numeric_limits<float>::max() };
			float maxX{ std::numeric_limits<float>::lowest() };
			float minY{ std::numeric_limits<float>::max() };
			float maxY{ std::numeric_limits<float>::lowest() };
			float minZ{ std::numeric_limits<float>::max() };
			float maxZ{ std::numeric_limits<float>::lowest() };

			for (Vec4& v : corners) {
				Vec4 trf{ lightView * v };
				minX = std::min(minX, trf.x);
				maxX = std::max(maxX, trf.x);
				minY = std::min(minY, trf.y);
				maxY = std::max(maxY, trf.y);
				minZ = std::min(minZ, trf.z);
				maxZ = std::max(maxZ, trf.z);
			}

			minZ = std::min(minZ, -far);
			maxZ = std::max(maxZ, far);

			Mat4 lightProjection{ Mat4::orthographic(minX, maxX, minY, maxY, minZ, maxZ) };

			return lightProjection * lightView;
		}

	};

}
