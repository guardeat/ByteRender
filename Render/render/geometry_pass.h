#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "core/transform.h"
#include "core/mesh.h"
#include "mesh_renderer.h"
#include "render_types.h"
#include "instance_group.h"
#include "material.h"
#include "texture.h"
#include "render_pass.h"
#include "render_context.h"
#include "render_data.h"
#include "framebuffer.h"
#include "camera.h"
#include "shader.h"

namespace Byte {

	class GeometryPass : public RenderPass<GeometryPass> {
	private:
		AssetID _geometryBuffer{};
		AssetID _geometryShader{};
		AssetID _instancedGeometryShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			auto [camera, cameraTransform] = context.camera();
			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };

			Mat4 projection{ camera.perspective(aspect) };
			Mat4 view{ cameraTransform.view() };

			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };

			data.device.framebuffer().bind(geometryBuffer);
			data.device.framebuffer().clearBuffer();

			Shader geometryShader{ data.shaders.at(_geometryShader) };
			data.device.shader().bind(geometryShader);

			for (auto [renderer, transform] : context.view<MeshRenderer, Transform>()) {
				if (renderer.mesh() == 0 || renderer.material() == 0 || !renderer.render()) {
					continue;
				}

				Mesh& mesh{ context.mesh(renderer.mesh()) };
				Material& material{ context.material(renderer.material()) };

				data.device.memory().bind(mesh);
				data.device.shader().set(geometryShader, transform);
				data.device.shader().set(geometryShader, "uProjection", projection);
				data.device.shader().set(geometryShader, "uView", view);
				data.device.shader().set(geometryShader, material, context.repository());

				data.device.framebuffer().draw(mesh.indexCount());
			}

			Shader instancedGeometryShader{ data.shaders.at(_instancedGeometryShader) };
			data.device.shader().bind(instancedGeometryShader);

			for (auto& [_, group] : context.instanceGroups()) {
				if (group.mesh() == 0 || group.material() == 0 || group.count() == 0 || !group.render()) {
					continue;
				}

				Mesh& mesh{ context.mesh(group.mesh()) };
				Material& material{ context.material(group.material()) };

				data.device.memory().bind(group);
				data.device.shader().set(instancedGeometryShader, "uProjection", projection);
				data.device.shader().set(instancedGeometryShader, "uView", view);
				data.device.shader().set(instancedGeometryShader, material, context.repository());

				data.device.framebuffer().draw(mesh.indexCount(), group.count());
			}
		}

		void initialize(RenderData& data) override {
			Framebuffer geometryBuffer{ data.width, data.height };

			Texture normalTexture{};
			normalTexture.attachment(AttachmentType::COLOR_0);
			normalTexture.internalFormat(ColorFormat::RGB16F);
			normalTexture.format(ColorFormat::RGB);
			normalTexture.dataType(DataType::FLOAT);
			geometryBuffer.texture(Tag{ "normal" }, std::move(normalTexture));

			Texture albedoTexture{};
			albedoTexture.attachment(AttachmentType::COLOR_1);
			albedoTexture.internalFormat(ColorFormat::RGB16F);
			albedoTexture.format(ColorFormat::RGB);
			albedoTexture.dataType(DataType::FLOAT);
			geometryBuffer.texture(Tag{ "albedo" }, std::move(albedoTexture));

			Texture materialTexture{};
			materialTexture.attachment(AttachmentType::COLOR_2);
			materialTexture.internalFormat(ColorFormat::RGBA);
			materialTexture.format(ColorFormat::RGBA);
			materialTexture.dataType(DataType::UNSIGNED_BYTE);
			geometryBuffer.texture(Tag{ "material" }, std::move(materialTexture));

			Texture depthTexture{};
			depthTexture.attachment(AttachmentType::DEPTH);
			depthTexture.internalFormat(ColorFormat::DEPTH);
			depthTexture.format(ColorFormat::DEPTH);
			depthTexture.dataType(DataType::FLOAT);
			geometryBuffer.texture(Tag{ "depth" }, std::move(depthTexture));

			_geometryBuffer = geometryBuffer.assetID();
			data.parameter("geometry_buffer_id", geometryBuffer.assetID());
			data.framebuffers.emplace(geometryBuffer.assetID(), std::move(geometryBuffer));

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader geometryShader{ shaderPath / "default.vert", shaderPath / "deferred.frag" };
			geometryShader.useDefaultMaterial(true);
			Shader instancedGeometryShader{ shaderPath / "instanced.vert", shaderPath / "deferred.frag" };
			instancedGeometryShader.useDefaultMaterial(true);

			_geometryShader = geometryShader.assetID();
			_instancedGeometryShader = instancedGeometryShader.assetID();

			data.parameter("geometry_shader_id", geometryShader.assetID());
			data.parameter("instanced_geometry_shader_id", instancedGeometryShader.assetID());

			data.shaders.emplace(geometryShader.assetID(), std::move(geometryShader));
			data.shaders.emplace(instancedGeometryShader.assetID(), std::move(instancedGeometryShader));
		}
	};

}
