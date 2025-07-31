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
		Map<AssetID, GPUBufferGroup> _meshes;
		Map<AssetID, GPUBufferGroup> _instanceGroups;
		Map<AssetID, GPUTexture> _textures;
		Map<AssetID, GPUShader> _shaders;
		Map<AssetID, GPUFramebuffer> _framebuffers;

	public:
		RenderDevice() = default;

		~RenderDevice() {
			clear();
		}

		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void load(Mesh& mesh) {
			GPUBufferGroup bufferGroup{ OpenGL::GMemory::build(mesh) };
			_meshes.emplace(mesh.assetID(), std::move(bufferGroup));
		}

		void load(InstanceGroup& instanced, Mesh& mesh) {
			if (!loaded(mesh)) {
				load(mesh);
			}
			


		}

		void load(Shader& shader) {
			uint32_t vertex{ OpenGL::GShader::compileShader(shader.vertex(),ShaderType::VERTEX)};
			uint32_t fragment{ OpenGL::GShader::compileShader(shader.fragment(),ShaderType::FRAGMENT)};
			uint32_t geometry{};

			if (!shader.geometry().empty()) {
				geometry = OpenGL::GShader::compileShader(shader.geometry(), ShaderType::GEOMETRY);
			}

			_shaders.emplace(shader.assetID(), OpenGL::GShader::buildProgram(vertex, fragment, geometry));
		}

		void load(Texture& texture) {
			GPUTexture id{ OpenGL::GTexture::build(texture) };
			_textures.emplace(texture.assetID(), id);
		}

		void load(Framebuffer& buffer) {
			auto [id, textures] = OpenGL::GFramebuffer::build(buffer);

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
			GPUBufferGroup& bufferGroup{ _meshes.at(mesh.assetID()) };
			OpenGL::GMemory::bind(bufferGroup.id);
		}

		void bind(const Shader& shader) {
			GPUShader id{ _shaders.at(shader.assetID()) };
			OpenGL::GShader::bind(id);
		}

		void bind(const InstanceGroup& group) {
			GPUBufferGroup bufferGroup{ _instanceGroups.at(group.assetID()) };
			OpenGL::GMemory::bind(bufferGroup.id);
		}

		void bind(const Texture& texture, TextureUnit unit = TextureUnit::UNIT_0) {
			GPUTexture id{ _textures.at(texture.assetID()) };
			OpenGL::GTexture::bind(id, unit);
		}

		void bind(const Framebuffer& buffer) {
			OpenGL::GFramebuffer::bind(buffer, _framebuffers.at(buffer.assetID()));
		}

		void bindDefault(size_t width, size_t height) {
			OpenGL::GFramebuffer::bind(width, height);
		}

		void clearBuffer() {
			OpenGL::clear();
		}

		void resize(Framebuffer& buffer, size_t width, size_t height) {
			if (buffer.resize()) {
				AssetID assetID{ buffer.assetID() };

				Vector<GPUResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textures.at(texture.assetID()).id);
					texture.width(width);
					texture.height(height);
					_textures.erase(texture.assetID());
				}

				OpenGL::GFramebuffer::release(_framebuffers.at(assetID), ids);
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
				OpenGL::GMemory::release(it->second);
				_meshes.erase(it);
			}
		}

		void release(InstanceGroup& group) {
			auto it{ _instanceGroups.find(group.assetID()) };
			if (it != _instanceGroups.end()) {
				OpenGL::GMemory::release(it->second);
				_instanceGroups.erase(it);
			}
		}

		void release(Shader& shader) {
			auto it{ _shaders.find(shader.assetID()) };
			if (it != _shaders.end()) {
				OpenGL::GShader::releaseProgram(it->second);
				_shaders.erase(it);
			}
		}

		void release(Texture& texture) {
			auto it{ _textures.find(texture.assetID()) };
			if (it != _textures.end()) {
				OpenGL::GTexture::release(it->second);
				_textures.erase(it);
			}
		}

		void release(Framebuffer& buffer) {
			auto it{ _framebuffers.find(buffer.assetID()) };
			if (it != _framebuffers.end()) {
				Vector<GPUResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textures.at(texture.assetID()).id);
				}

				OpenGL::GFramebuffer::release(it->second, ids);
				_framebuffers.erase(it);
			}
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			GPUShader id{ _shaders.at(shader.assetID()) };
			OpenGL::GShader::uniform(id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			GPUShader id{ _shaders.at(shader.assetID()) };

			if (shader.uniforms().contains("uColor")) {
				OpenGL::GShader::uniform(id, "uColor", material.color());
			}

			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &id, &shader](const auto& inputValue) {
						OpenGL::GShader::uniform(id, tag, inputValue);
						}, input);
				}
			}
		}

		void uniform(const Shader& shader, const Transform& transform) {
			GPUShader id{ _shaders.at(shader.assetID()) };
			OpenGL::GShader::uniform(id, "uPosition", transform.position());
			OpenGL::GShader::uniform(id, "uScale", transform.scale());
			OpenGL::GShader::uniform(id, "uRotation", transform.rotation());
		}

		void uniform(
			const Shader& shader, 
			const Tag& uniform,
			const Texture& texture, 
			TextureUnit unit = TextureUnit::UNIT_0) {
			bind(texture, unit);

			GPUShader id{ _shaders.at(shader.assetID()) };
			OpenGL::GShader::uniform(id, uniform, static_cast<int>(unit));
		}

		void updateBuffer(InstanceGroup& group) {

		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::GDraw::elements(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::GDraw::elementsInstanced(size, instanceCount, drawType);
		}

		void state(RenderState state) {
			OpenGL::state(state);
		}

		void clear() {
			for (auto& [assetID, group] : _meshes) {
				OpenGL::GMemory::release(group);
			}
			_meshes.clear();

			for (auto& [renderID, group] : _instanceGroups) {
				OpenGL::GMemory::release(group);
			}
			_instanceGroups.clear();

			for (auto& [assetID, shader] : _shaders) {
				OpenGL::GShader::releaseProgram(shader);
			}
			_shaders.clear();

			for (auto& [assetID, texture] : _textures) {
				OpenGL::GTexture::release(texture);
			}
			_textures.clear();

			for (auto& [tag, buffer] : _framebuffers) {
				OpenGL::GFramebuffer::release(buffer, Vector<GPUResourceID>{});
			}
			_framebuffers.clear();
		}

	};

}
