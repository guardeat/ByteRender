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
			Path shaderPath{ data.parameter<Path>("default_shader_path") };
			Shader skyboxShader{ shaderPath/"skybox.vert",shaderPath/"skybox.frag" };
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
			if(!data.parameter<bool>("render_shadow")) {
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

			for(size_t idx{}; idx < data.parameter<uint64_t>("cascade_count"); ++idx) {
				Framebuffer& shadowBuffer{ data.framebuffers.at(_shadowBuffers[idx])};
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };

				data.device.bind(shadowBuffer);
				data.device.clearBuffer();
				
				Shader& shadowShader{ data.shaders.at(_shadowShader) };
				data.device.bind(shadowShader);
				data.device.uniform(shadowShader, "uLightSpace", lightSpace);
				for(auto[renderer, transform] : context.view<MeshRenderer, Transform>()) {
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
					if(group.mesh() == 0 || group.count() == 0 || !group.shadow()) {
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
			Shader shadowShader{ shaderPath/"depth.vert", shaderPath/"depth.frag" };
			Shader instancedShadowShader{ shaderPath/"instanced_depth.vert", shaderPath/"depth.frag" };

			_shadowShader = shadowShader.assetID();
			_instancedShadowShader = instancedShadowShader.assetID();

			data.shaders.emplace(shadowShader.assetID(), std::move(shadowShader));	
			data.shaders.emplace(instancedShadowShader.assetID(), std::move(instancedShadowShader));

			constexpr size_t CASCADE_COUNT{ 4 };
			data.parameter("cascade_count", CASCADE_COUNT);

			constexpr size_t SHADOW_BUFFER_SIZE{ 2048 };
			data.parameter("shadow_buffer_size", SHADOW_BUFFER_SIZE);

			for(size_t idx{}; idx < CASCADE_COUNT; ++idx) {
				float x{ static_cast<float>(idx) };
				float divisor{ 0.833f * x * x * x - 0.25f * x * x + 0.417f * x + 1.0f };
				data.parameter("cascade_divisor_" + std::to_string(idx), divisor);
				data.parameter("light_space_matrix_" + std::to_string(idx), Mat4{});
				Framebuffer buffer{SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE};
				buffer.resize(false);

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

			for(auto[renderer, transform] : context.view<MeshRenderer, Transform>()) {
				if (renderer.mesh() == 0 || renderer.material() == 0 || !renderer.render()) {
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
				if(group.mesh() == 0 || group.material() == 0 || group.count() == 0 || !group.render()) {
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

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader geometryShader{ shaderPath/"default.vert", shaderPath/"deferred.frag" };
			geometryShader.useMaterial(true);
			Shader instancedGeometryShader{ shaderPath/"instanced.vert", shaderPath/"deferred.frag" };
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

			data.device.bind(colorBuffer);

			Mesh& quad{ data.meshes.at(_quad) };
			Shader& lightingShader{ data.shaders.at(_lightingShader) };

			data.device.bind(quad);
			data.device.bind(lightingShader);

			data.device.uniform(lightingShader, "uNormal", geometryBuffer.texture("normal"));
			data.device.uniform(lightingShader, "uAlbedo", geometryBuffer.texture("albedo"), TextureUnit::UNIT_1);
			data.device.uniform(lightingShader, "uMaterial", geometryBuffer.texture("material"), TextureUnit::UNIT_2);
			data.device.uniform(lightingShader, "uDepth", geometryBuffer.texture("depth"), TextureUnit::UNIT_3);
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

			data.device.uniform(lightingShader, "uDLight.direction", dLightTransform.front());
			data.device.uniform(lightingShader, "uDLight.color", directionalLight.color);
			data.device.uniform(lightingShader, "uDLight.intensity", directionalLight.intensity);

			data.device.uniform(lightingShader, "uView", view);
			data.device.uniform(lightingShader, "uInverseView", inverseView);
			data.device.uniform(lightingShader, "uInverseProjection", inverseProjection);
			data.device.uniform(lightingShader, "uViewPos", cameraTransform.position());

			size_t cascadeCount{ data.parameter<uint64_t>("cascade_count") };
			data.device.uniform(lightingShader, "uCascadeCount", cascadeCount);

			for (size_t idx{}; idx < cascadeCount; ++idx) {
				Mat4 lightSpace{ data.parameter<Mat4>("light_space_matrix_" + std::to_string(idx)) };
				float cascadeDivisor{ data.parameter<float>("cascade_divisor_" + std::to_string(idx)) };
				float cascadeFar{ camera.farPlane() / cascadeDivisor };
				AssetID shadowBufferID{ data.parameter<AssetID>("shadow_buffer_id_" + std::to_string(idx)) };
				Texture& depthTexture{ data.framebuffers.at(shadowBufferID).texture("depth") };
				TextureUnit unit{ static_cast<TextureUnit>(static_cast<size_t>(TextureUnit::UNIT_4) + idx) };

				data.device.uniform(lightingShader, "uLightSpaces[" + std::to_string(idx) + "]", lightSpace);
				data.device.uniform(lightingShader, "uCascadeFars[" + std::to_string(idx) + "]", cascadeFar);
				data.device.uniform(lightingShader, "uDepthMaps[" + std::to_string(idx) + "]", depthTexture, unit);
			}
		}

		void drawDirectionalLight(RenderData& data) {
			Mesh& quad{ data.meshes.at(_quad) };
			Shader& lightingShader{ data.shaders.at(_lightingShader) };
			data.device.bind(quad);
			data.device.bind(lightingShader);
			data.device.draw(quad.indexCount());
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

			data.device.bind(pointLightShader);
			data.device.bind(pointLightGroup);

			data.device.uniform(pointLightShader, "uProjection", camera.perspective(aspect));
			data.device.uniform(pointLightShader, "uView", cameraTransform.view());
			data.device.uniform(pointLightShader, "uInverseView", cameraTransform.view().inverse());
			data.device.uniform(pointLightShader, "uInverseProjection", camera.perspective(aspect).inverse());
			data.device.uniform(pointLightShader, "uViewPos", cameraTransform.position());
			data.device.uniform(pointLightShader, "uViewPortSize", viewPortSize);

			Framebuffer& geometryBuffer{ data.framebuffers.at(_geometryBuffer) };
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
	};

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

			data.device.bind(bloomBuffer);
			data.device.clearBuffer();

			Mesh& quad{ data.meshes.at(_quad) };
			Shader& downShader{ data.shaders.at(_bloomDownShader) };

			float gamma{ data.parameter<float>("gamma") };
			float width{ static_cast<float>(colorBuffer.width()) };
			float height{ static_cast<float>(colorBuffer.height()) };

			data.device.bind(downShader);
			data.device.uniform(downShader, "uInverseGamma", 1.0f / gamma);
			data.device.uniform(downShader, "uKarisAverage", true);
			data.device.uniform(downShader, "uSrcResolution", Vec2{ width,height });
			data.device.uniform(downShader, "uSrcTexture", colorBuffer.texture("color"));

			data.device.bind(quad);
			data.device.draw(quad.indexCount());

			for (size_t i{ 1 }; i < mipCount; ++i) {
				Texture& srcTexture{ bloomBuffer.texture("bloom") };

				width = static_cast<float>(bloomBuffer.width());
				height = static_cast<float>(bloomBuffer.height());

				Framebuffer& bloomBuffer{ data.framebuffers.at(_bloomBuffers.at(i)) };

				data.device.bind(bloomBuffer);
				data.device.clearBuffer();

				data.device.uniform(downShader, "uSrcTexture", srcTexture);
				data.device.uniform(downShader, "uSrcResolution", Vec2{ width, height });

				data.device.draw(quad.indexCount());

				data.device.uniform(downShader,"uKarisAverage",false);
			}

			data.device.state(RenderState::DISABLE_DEPTH);
			data.device.state(RenderState::ENABLE_BLEND);

			Shader& upShader{ data.shaders.at(_bloomUpShader) };
			data.device.bind(upShader);
			data.device.uniform(upShader, "uFilterRadius", 0.01f);

			for (size_t i{ mipCount - 1 }; i > 0; --i) {
				Framebuffer& sourceBuffer{ data.framebuffers.at(_bloomBuffers.at(i)) };
				Texture& srcTexture{ sourceBuffer.texture("bloom") };

				Framebuffer& bloomBuffer{ data.framebuffers.at(_bloomBuffers.at(i - 1)) };

				data.device.bind(bloomBuffer);
				data.device.clearBuffer();

				data.device.uniform(upShader, "uSrcTexture", srcTexture);

				data.device.draw(quad.indexCount());
			}

			float strength{ data.parameter<float>("bloom_strength") };

			data.device.blendWeights(strength,1 - strength);
			data.device.state(RenderState::BLEND_WEIGHTED);

			data.device.bind(colorBuffer);
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

			constexpr size_t MIPMAP_LEVELS{ 4 };

			data.parameter("bloom_mipmap_levels", MIPMAP_LEVELS);

			float divisor{ 2.0f };
			for(size_t idx{}; idx < MIPMAP_LEVELS; ++idx) {
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

			Shader bloomUpShader{ shaderPath/"quad.vert", shaderPath/"bloom_upsample.frag" };
			_bloomUpShader = bloomUpShader.assetID();
			data.shaders.emplace(bloomUpShader.assetID(), std::move(bloomUpShader));

			Shader bloomDownShader{ shaderPath/"quad.vert", shaderPath/"bloom_downsample.frag" };
			_bloomDownShader = bloomDownShader.assetID();
			data.shaders.emplace(bloomDownShader.assetID(), std::move(bloomDownShader));

			data.parameter("bloom_down_shader_id", _bloomDownShader);
			data.parameter("bloom_up_shader_id", _bloomUpShader);
			data.parameter("render_bloom", true);
			data.parameter("bloom_strength", 0.3f);
		}
	};

	class DrawPass: public RenderPass<DrawPass> {
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
				data.device.bind(*shader);

				float width{ static_cast<float>(data.width) };
				float height{ static_cast<float>(data.height) };
				data.device.uniform(*shader, "uScreenSize", Vec2{ width,height });
			}
			else {
				shader = &data.shaders.at(_finalShader);
				data.device.bind(*shader);
			}

			auto [camera, _] = context.camera();

			data.device.bindDefault(data.width, data.height);

			data.device.bind(quad);
			
			data.device.uniform(*shader, "uColor", colorBuffer.texture("color"));
			data.device.uniform(*shader, "uDepth", geometryBuffer.texture("depth"), TextureUnit::UNIT_1);
			data.device.uniform(*shader, "uGamma", data.parameter<float>("gamma"));
			data.device.uniform(*shader, "uFar", camera.farPlane());
			data.device.uniform(*shader, "uNear", camera.nearPlane());
			data.device.uniform(*shader, "uFogColor", data.parameter<Vec3>("fog_color"));
			data.device.uniform(*shader, "uFogNear", data.parameter<float>("fog_near"));
			data.device.uniform(*shader, "uFogFar", data.parameter<float>("fog_far"));

			data.device.draw(quad.indexCount());
		}

		void initialize(RenderData& data) override {
			_colorBuffer = data.parameter<AssetID>("color_buffer_id");
			_geometryBuffer = data.parameter<AssetID>("geometry_buffer_id");
			_quad = data.parameter<AssetID>("quad_mesh_id");

			Path shaderPath{ data.parameter<Path>("default_shader_path") };

			Shader finalShader{ shaderPath/"quad.vert",shaderPath/"final.frag" };
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