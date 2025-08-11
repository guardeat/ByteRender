#pragma once

#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/transform.h"
#include "opengl_api.h"
#include "render_types.h"
#include "instance_group.h"
#include "shader.h"
#include "texture.h"

namespace Byte {

	class RenderDevice {
	private:
		Map<AssetID, GBufferGroup> _meshes;
		Map<AssetID, GBufferGroup> _instanceGroups;
		Map<AssetID, GTexture> _textures;
		Map<AssetID, GShader> _shaders;
		Map<AssetID, GFramebuffer> _framebuffers;

	public:
		RenderDevice() = default;

		~RenderDevice() {
			clear();
		}

		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void load(Mesh& mesh) {
			GBufferGroup bufferGroup{ OpenGL::build(mesh) };
			_meshes.emplace(mesh.assetID(), std::move(bufferGroup));
		}

		void load(InstanceGroup& instanced, Mesh& mesh) {
			GBufferGroup group{ OpenGL::build(instanced, mesh) };
			group.capacity = instanced.count();
			_instanceGroups.emplace(instanced.assetID(), std::move(group));

			instanced.sync();
		}

		void load(Shader& shader) {
			uint32_t vertex{ OpenGL::compileShader(shader.vertex(),ShaderType::VERTEX)};
			uint32_t fragment{ OpenGL::compileShader(shader.fragment(),ShaderType::FRAGMENT)};
			uint32_t geometry{};

			if (!shader.geometry().empty()) {
				geometry = OpenGL::compileShader(shader.geometry(), ShaderType::GEOMETRY);
			}

			_shaders.emplace(shader.assetID(), OpenGL::build(vertex, fragment, geometry));
		}

		void load(Texture& texture) {
			GTexture id{ OpenGL::build(texture) };
			_textures.emplace(texture.assetID(), id);
		}

		void load(Framebuffer& buffer) {
			auto [id, textures] = OpenGL::build(buffer);

			for (auto [assetID, gID] : textures) {
				_textures.emplace(assetID, gID);
			}

			_framebuffers.emplace(buffer.assetID(), id);
		}

		bool loaded(Mesh& mesh) {
			return _meshes.contains(mesh.assetID());
		}

		bool loaded(InstanceGroup& group) {
			return _instanceGroups.contains(group.assetID());
		}

		bool loaded(Shader& shader) {
			return _shaders.contains(shader.assetID());
		}

		bool loaded(Texture& texture) {
			return _textures.contains(texture.assetID());
		}

		bool loaded(Framebuffer& buffer) {
			return _framebuffers.contains(buffer.assetID());
		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void bind(const Mesh& mesh) {
			GBufferGroup& bufferGroup{ _meshes.at(mesh.assetID()) };
			OpenGL::bind(bufferGroup);
		}

		void bind(const Shader& shader) {
			GShader id{ _shaders.at(shader.assetID()) };
			OpenGL::bind(id);
		}

		void bind(const InstanceGroup& group) {
			GBufferGroup bufferGroup{ _instanceGroups.at(group.assetID()) };
			OpenGL::bind(bufferGroup);
		}

		void bind(const Texture& texture, TextureUnit unit = TextureUnit::UNIT_0) {
			GTexture id{ _textures.at(texture.assetID()) };
			OpenGL::bind(id, unit);
		}

		void bind(const Framebuffer& buffer) {
			OpenGL::bind(buffer, _framebuffers.at(buffer.assetID()));
		}

		void bindDefault(size_t width, size_t height) {
			OpenGL::bind(width, height);
		}

		void viewport(size_t width, size_t height) {
			OpenGL::viewport(width, height);
		}

		void blendWeights(float source, float destination) {
			OpenGL::blendWeights(source, destination);
		}

		void clearBuffer() {
			OpenGL::clear();
		}

		void resize(Framebuffer& buffer, size_t width, size_t height) {
			if (buffer.resize()) {
				AssetID assetID{ buffer.assetID() };

				Vector<GResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textures.at(texture.assetID()).id);

					texture.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
					texture.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
					_textures.erase(texture.assetID());
				}

				OpenGL::release(_framebuffers.at(assetID), ids);
				_framebuffers.erase(assetID);

				buffer.attachments().clear();
				buffer.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
				buffer.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
				
				load(buffer);
			}
		}

		void release(Mesh& mesh) {
			auto it{ _meshes.find(mesh.assetID()) };
			if (it != _meshes.end()) {
				OpenGL::release(it->second);
				_meshes.erase(it);
			}
		}

		void release(InstanceGroup& group) {
			auto it{ _instanceGroups.find(group.assetID()) };
			if (it != _instanceGroups.end()) {
				OpenGL::release(it->second);
				_instanceGroups.erase(it);
			}
		}

		void release(Shader& shader) {
			auto it{ _shaders.find(shader.assetID()) };
			if (it != _shaders.end()) {
				OpenGL::release(it->second);
				_shaders.erase(it);
			}
		}

		void release(Texture& texture) {
			auto it{ _textures.find(texture.assetID()) };
			if (it != _textures.end()) {
				OpenGL::release(it->second);
				_textures.erase(it);
			}
		}

		void release(Framebuffer& buffer) {
			auto it{ _framebuffers.find(buffer.assetID()) };
			if (it != _framebuffers.end()) {
				Vector<GResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textures.at(texture.assetID()).id);
				}

				OpenGL::release(it->second, ids);
				_framebuffers.erase(it);
			}
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			GShader id{ _shaders.at(shader.assetID()) };
			OpenGL::uniform(id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			GShader id{ _shaders.at(shader.assetID()) };

			if (shader.useDefaultMaterial()) {
				OpenGL::uniform(id, "uAlbedo", material.color());

				OpenGL::uniform(id, "uMetallic", material.metallic());
				OpenGL::uniform(id, "uRoughness", material.roughness());

				OpenGL::uniform(id, "uEmission", material.emission());
				OpenGL::uniform(id, "uAO", material.ambientOcclusion());
			}

			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &id, &shader](const auto& inputValue) {
						OpenGL::uniform(id, tag, inputValue);
						}, input);
				}
			}
		}

		void uniform(const Shader& shader, const Transform& transform) {
			GShader id{ _shaders.at(shader.assetID()) };
			OpenGL::uniform(id, "uPosition", transform.position());
			OpenGL::uniform(id, "uScale", transform.scale());
			OpenGL::uniform(id, "uRotation", transform.rotation());
		}

		void uniform(
			const Shader& shader, 
			const Tag& uniform,
			const Texture& texture, 
			TextureUnit unit = TextureUnit::UNIT_0) {
			bind(texture, unit);

			GShader id{ _shaders.at(shader.assetID()) };
			OpenGL::uniform(id, uniform, static_cast<int>(unit));
		}

		void updateBuffer(InstanceGroup& group, float capacityMultiplier = 2.0f) {
			GBufferGroup& bufferGroup{ _meshes.at(group.mesh()) };
			size_t size{ group.data().size() };
			if (size > bufferGroup.capacity) {
				size_t newSize{ static_cast<size_t>(group.data().size() * capacityMultiplier) };
				bufferGroup.capacity = newSize;
				OpenGL::bufferData(bufferGroup.renderBuffers[0], group.data(), newSize, false);
			}
			else {
				OpenGL::subBufferData(bufferGroup.renderBuffers[0], group.data());
			}

			group.sync();
		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::draw(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::draw(size, instanceCount, drawType);
		}

		void renderState(RenderState state) {
			OpenGL::renderState(state);
		}

		void clear() {
			for (auto& [assetID, group] : _meshes) {
				OpenGL::release(group);
			}
			_meshes.clear();

			for (auto& [renderID, group] : _instanceGroups) {
				OpenGL::release(group);
			}
			_instanceGroups.clear();

			for (auto& [assetID, shader] : _shaders) {
				OpenGL::release(shader);
			}
			_shaders.clear();

			for (auto& [assetID, texture] : _textures) {
				OpenGL::release(texture);
			}
			_textures.clear();

			for (auto& [tag, buffer] : _framebuffers) {
				OpenGL::release(buffer, Vector<GResourceID>{});
			}
			_framebuffers.clear();
		}

	};

}
