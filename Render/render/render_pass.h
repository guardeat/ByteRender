#pragma once

#include "core/core_types.h"
#include "core/transform.h"
#include "render_context.h"
#include "render_data.h"
#include "camera.h"
#include "mesh_renderer.h"

namespace Byte {

	class IRenderPass {
	public:
		virtual ~IRenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}

		virtual void terminate(RenderData& data) {
		}

		virtual UniquePtr<IRenderPass> clone() const = 0;
	};

	template<typename Derived>
	class RenderPass : public IRenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}
		
		virtual void terminate(RenderData& data) {
		}

		UniquePtr<IRenderPass> clone() const override {
			return std::make_unique<Derived>(static_cast<const Derived&>(*this));
		}
	};

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

			data.device.uniform(skyboxShader, skyboxMaterial);
			data.device.uniform(skyboxShader, "uDLight.direction", dLightTransform.front());
			data.device.uniform(skyboxShader, "uDLight.color", dLight.color);
			data.device.uniform(skyboxShader, "uDLight.intensity", dLight.intensity);
			data.device.uniform(skyboxShader, "uInverseViewProjection", inverseViewProjection);

			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.draw(quad.indexCount());
			data.device.state(RenderState::ENABLE_DEPTH);
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

	class ShadowPass : public RenderPass<ShadowPass> {
	private:
		Vector<AssetID> _shadowBuffers{};
		AssetID _shadowShader{};
		AssetID _instancedShadowShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
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

			for(size_t idx{}; idx < data.parameter<uint64_t>("cascade_count"); ++idx) {
				Framebuffer& shadowBuffer{ data.framebuffers.at(_shadowBuffers[idx])};
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };

				data.device.bind(shadowBuffer);
				data.device.clearBuffer();
				
				Shader& shadowShader{ data.shaders.at(_shadowShader) };
				data.device.bind(shadowShader);
				data.device.uniform(shadowShader, "uLightSpace", lightSpace);
				for(auto[renderer, transform] : context.view<MeshRenderer, Transform>()) {
					if (renderer.mesh() == 0 || renderer.material() == 0) {
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
					if(group.mesh() == 0 || group.count() == 0) {
						continue;
					}
					Mesh& mesh{ context.mesh(group.mesh()) };
					data.device.bind(group);
	
					data.device.draw(mesh.indexCount(), group.count());
				}
			}
		}

		void initialize(RenderData& data) override {
			Shader shadowShader{ "../Render/shader/depth.vert", "../Render/shader/depth.frag" };
			Shader instancedShadowShader{ "../Render/shader/instanced_depth.vert", "../Render/shader/depth.frag" };

			_shadowShader = shadowShader.assetID();
			_instancedShadowShader = instancedShadowShader.assetID();

			data.shaders.emplace(shadowShader.assetID(), std::move(shadowShader));	
			data.shaders.emplace(instancedShadowShader.assetID(), std::move(instancedShadowShader));

			constexpr size_t CASCADE_COUNT{ 4 };
			data.parameter("cascade_count", CASCADE_COUNT);

			constexpr size_t SHADOW_BUFFER_SIZE{ 1024 };
			data.parameter("shadow_buffer_size", SHADOW_BUFFER_SIZE);

			for(size_t idx{}; idx < CASCADE_COUNT; ++idx) {
				float x{ static_cast<float>(idx) };
				float divisor{ 0.833f * x * x * x - 0.25f * x * x + 0.417f * x + 1.0f };
				data.parameter("cascade_divisor_" + std::to_string(idx), divisor);
				data.parameter("light_space_matrix_" + std::to_string(idx), Mat4{});
				Framebuffer buffer{SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE};

				Texture depthTexture{};
				depthTexture.attachment(AttachmentType::DEPTH);
				depthTexture.internalFormat(ColorFormat::DEPTH);
				depthTexture.format(ColorFormat::DEPTH);
				depthTexture.dataType(DataType::FLOAT);

				buffer.texture(Tag{ "depth" }, std::move(depthTexture));

				_shadowBuffers.push_back(buffer.assetID());
				data.parameter(Tag{ "shadow_buffer_id_" } + std::to_string(idx), buffer.assetID());
				data.framebuffers.emplace(buffer.assetID(), buffer);
			}
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

			data.device.bind(geometryBuffer);
			data.device.clearBuffer();
		
			Shader geometryShader{ data.shaders.at(_geometryShader) };
			data.device.bind(geometryShader);

			for(auto[id, renderer, transform] : context.view<EntityID, MeshRenderer, Transform>()) {
				if (renderer.mesh() == 0 || renderer.material() == 0) {
					continue;
				}

				Mesh& mesh{ context.mesh(renderer.mesh()) };
				Material& material{ context.material(renderer.material()) };

				data.device.bind(mesh);
				data.device.uniform(geometryShader, transform);
				data.device.uniform(geometryShader, "uProjection", projection);
				data.device.uniform(geometryShader, "uView", view);
				data.device.uniform(geometryShader, material);
				
				data.device.draw(mesh.indexCount());
			}

			Shader instancedGeometryShader{ data.shaders.at(_instancedGeometryShader) };
			data.device.bind(instancedGeometryShader);

			for (auto& [_, group] : context.instanceGroups()) {
				if(group.mesh() == 0 || group.material() == 0 || group.count() == 0) {
					continue;
				}

				Mesh& mesh{ context.mesh(group.mesh()) };
				Material& material{ context.material(group.material()) };

				data.device.bind(group);
				data.device.uniform(instancedGeometryShader, "uProjection", projection);
				data.device.uniform(instancedGeometryShader, "uView", view);
				data.device.uniform(instancedGeometryShader, material);

				data.device.draw(mesh.indexCount(), group.count());
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

			Shader geometryShader{ "../Render/shader/default.vert", "../Render/shader/deferred.frag" };
			geometryShader.useMaterial(true);
			Shader instancedGeometryShader{ "../Render/shader/instanced.vert", "../Render/shader/deferred.frag" };
			instancedGeometryShader.useMaterial(true);

			_geometryShader = geometryShader.assetID();
			_instancedGeometryShader = instancedGeometryShader.assetID();

			data.parameter("geometry_shader_id", geometryShader.assetID());
			data.parameter("instanced_geometry_shader_id", instancedGeometryShader.assetID());

			data.shaders.emplace(geometryShader.assetID(), std::move(geometryShader));
			data.shaders.emplace(instancedGeometryShader.assetID(), std::move(instancedGeometryShader));
		}
	};

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
			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };
			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };

			data.device.bind(colorBuffer);
			
			auto [directionalLight, dLightTransform] = context.directionalLight();
			auto [camera, cameraTransform] = context.camera();
			float aspect{ static_cast<float>(data.width) / static_cast<float>(data.height) };

			Mat4 view{ cameraTransform.view() };
			Mat4 inverseView{ view.inverse() };
			Mat4 projection{ camera.perspective(aspect) };
			Mat4 inverseProjection{ projection.inverse() };

			Mesh& quad{ data.meshes.at(_quad) };

			Shader& lightingShader{ data.shaders.at(_lightingShader) };
			data.device.bind(quad);
			data.device.bind(lightingShader);

			data.device.uniform(lightingShader, "uNormal", geometryBuffer.texture("normal"));
			data.device.uniform(lightingShader, "uAlbedo", geometryBuffer.texture("albedo"), TextureUnit::UNIT_1);
			data.device.uniform(lightingShader, "uMaterial", geometryBuffer.texture("material"), TextureUnit::UNIT_2);
			data.device.uniform(lightingShader, "uDepth", geometryBuffer.texture("depth"), TextureUnit::UNIT_3);

			data.device.uniform(lightingShader, "uDLight.direction", dLightTransform.front());
			data.device.uniform(lightingShader, "uDLight.color", directionalLight.color);
			data.device.uniform(lightingShader, "uDLight.intensity", directionalLight.intensity);

			data.device.uniform(lightingShader, "uView", view);
			data.device.uniform(lightingShader, "uInverseView", inverseView);
			data.device.uniform(lightingShader, "uInverseProjection", inverseProjection);
			data.device.uniform(lightingShader, "uViewPos", cameraTransform.position());

			size_t cascadeCount{ data.parameter<uint64_t>("cascade_count") };

			for(size_t idx{}; idx < cascadeCount; ++idx) {
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };
				float cascadeDivisor{ data.parameter<float>("cascade_divisor_" + std::to_string(idx)) };
				float cascadeFar{ camera.farPlane() / cascadeDivisor };
				AssetID shadowBufferID{ data.parameter<AssetID>("shadow_buffer_id_" + std::to_string(idx)) };
				Texture& depthTexture{ data.framebuffers.at(shadowBufferID).texture("depth")};
				TextureUnit unit{ static_cast<TextureUnit>(static_cast<size_t>(TextureUnit::UNIT_4) + idx) };
				data.device.uniform(lightingShader, "uLightSpaces[" + std::to_string(idx) + "]", lightSpace);
				data.device.uniform(lightingShader, "uCascadeFars[" + std::to_string(idx) + "]", cascadeFar);
				data.device.uniform(lightingShader, "uDepthMaps[" + std::to_string(idx) + "]",depthTexture, unit);
				data.device.uniform(lightingShader, "uCascadeCount", data.parameter<uint64_t>("cascade_count"));
			}

			data.device.draw(quad.indexCount());

			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.state(RenderState::ENABLE_BLEND);
			data.device.state(RenderState::BLEND_ADD);
			data.device.state(RenderState::ENABLE_CULLING);
			data.device.state(RenderState::CULL_FRONT);

			InstanceGroup& pointLightGroup{ context.instanceGroup(_pointLightGroup) };
			Mesh& pointLightMesh{ context.mesh(pointLightGroup.mesh()) };

			Shader& pointLightShader{ data.shaders.at(_pointLightShader) };
			Vec2 viewPortSize{ static_cast<float>(data.width), static_cast<float>(data.height) };

			data.device.bind(pointLightShader);
			data.device.bind(pointLightGroup);

			data.device.uniform(pointLightShader, "uProjection", camera.perspective(aspect));
			data.device.uniform(pointLightShader, "uView", view);
			data.device.uniform(pointLightShader, "uProjection", projection);
			data.device.uniform(pointLightShader, "uInverseView", inverseView);
			data.device.uniform(pointLightShader, "uInverseProjection", inverseProjection);
			data.device.uniform(pointLightShader, "uViewPos", cameraTransform.position());
			data.device.uniform(pointLightShader, "uViewPortSize", viewPortSize);

			data.device.uniform(pointLightShader, "uNormal", geometryBuffer.texture("normal"), TextureUnit::UNIT_0);
			data.device.uniform(pointLightShader, "uAlbedo", geometryBuffer.texture("albedo"), TextureUnit::UNIT_1);
			data.device.uniform(pointLightShader, "uMaterial", geometryBuffer.texture("material"), TextureUnit::UNIT_2);
			data.device.uniform(pointLightShader, "uDepth", geometryBuffer.texture("depth"), TextureUnit::UNIT_3);

			data.device.draw(pointLightMesh.indexCount(), pointLightGroup.count(), DrawType::TRIANGLES);

			data.device.state(RenderState::ENABLE_DEPTH);
			data.device.state(RenderState::DISABLE_BLEND);
			data.device.state(RenderState::CULL_BACK);
			data.device.state(RenderState::DISABLE_CULLING);
		}

		void initialize(RenderData& data) override {
			_geometryBuffer = data.parameter<AssetID>("geometry_buffer_id");
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Shader lightingShader{ "../Render/shader/quad.vert", "../Render/shader/lighting.frag" };
			_lightingShader = lightingShader.assetID();
			data.parameter("lighting_shader_id", lightingShader.assetID());
			data.shaders.emplace(lightingShader.assetID(), std::move(lightingShader));

			Shader pointLightShader{ "../Render/shader/point_light.vert", "../Render/shader/point_light.frag" };
			_pointLightShader = pointLightShader.assetID();
			data.parameter("point_light_shader_id", pointLightShader.assetID());
			data.shaders.emplace(pointLightShader.assetID(), std::move(pointLightShader));

			_pointLightGroup = data.parameter<AssetID>("point_light_group_id");
		}
	};

	class DrawPass: public RenderPass<DrawPass> {
	private:
		AssetID _colorBuffer{};
		AssetID _quad{};
		AssetID _finalShader{};

	public:
		void render(RenderData& data, RenderContext& context) override {
			Framebuffer& colorBuffer{ data.framebuffers.at(_colorBuffer) };
			
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& finalShader{ data.shaders.at(_finalShader) };

			data.device.bindDefault(data.width, data.height);

			data.device.bind(quad);
			data.device.bind(finalShader);
			
			data.device.uniform(finalShader, "uAlbedo", colorBuffer.texture("color"));
			data.device.uniform(finalShader, "uGamma", data.parameter<float>("gamma"));
			data.device.draw(quad.indexCount());
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