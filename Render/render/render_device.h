#pragma once

#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/transform.h"
#include "opengl_api.h"
#include "render_types.h"
#include "render_array.h"
#include "instance_group.h"
#include "shader.h"
#include "texture.h"

namespace Byte {

	class RenderDevice {
	private:
		Map<AssetID, RenderArray> _meshArrays;
		Map<AssetID, RenderArray> _instancedArrays;
		Map<AssetID, TextureID> _textureIDs;
		Map<AssetID, ShaderID> _shaderIDs;
		Map<AssetID, FramebufferID> _framebufferIDs;

		size_t _maxInactiveFrames{ 10 };

	public:
		RenderDevice(size_t maxInactiveFrames = 10)
			: _maxInactiveFrames(maxInactiveFrames) {
		}

		~RenderDevice() {
			clear();
		}

		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void load(Mesh& mesh) {
			RenderArray meshArray{ OpenGL::GMemory::build(mesh) };
			_meshArrays.emplace(mesh.assetID(), std::move(meshArray));
		}

		void load(InstanceGroup& instanced, Mesh& mesh) {

		}

		void load(Shader& shader) {
			uint32_t vertex{ OpenGL::GShader::compileShader(shader.vertex(),ShaderType::VERTEX)};
			uint32_t fragment{ OpenGL::GShader::compileShader(shader.fragment(),ShaderType::FRAGMENT)};
			uint32_t geometry{};

			if (!shader.geometry().empty()) {
				geometry = OpenGL::GShader::compileShader(shader.geometry(), ShaderType::GEOMETRY);
			}

			_shaderIDs.emplace(shader.assetID(), OpenGL::GShader::buildProgram(vertex, fragment, geometry));
		}

		void load(Texture& texture) {
			TextureID id{ OpenGL::GTexture::build(texture) };
			_textureIDs.emplace(texture.assetID(), id);
		}

		void load(Framebuffer& buffer) {
			auto [id, textures] = OpenGL::GFramebuffer::build(buffer);

			for (auto [assetID, gID] : textures) {
				_textureIDs.emplace(assetID, gID);
			}

			_framebufferIDs.emplace(buffer.assetID(), id);
		}

		bool loaded(Mesh& mesh) {
			return _meshArrays.contains(mesh.assetID());
		}

		bool loaded(InstanceGroup& group) {
			return _instancedArrays.contains(group.assetID());
		}

		bool loaded(Shader& shader) {
			return _shaderIDs.contains(shader.assetID());
		}

		bool loaded(Texture& texture) {
			return _textureIDs.contains(texture.assetID());
		}

		bool loaded(Framebuffer& buffer) {
			return _framebufferIDs.contains(buffer.assetID());
		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void bind(const Mesh& mesh) {
			RenderArray& renderArray{ _meshArrays.at(mesh.assetID()) };
			OpenGL::GMemory::bind(renderArray.id);
		}

		void bind(const Shader& shader) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::GShader::bind(id);
		}

		void bind(const InstanceGroup& group) {
			RenderArray renderArray{ _instancedArrays.at(group.assetID()) };
			OpenGL::GMemory::bind(renderArray.id);
		}

		void bind(const Texture& texture, TextureUnit unit = TextureUnit::UNIT_0) {
			TextureID id{ _textureIDs.at(texture.assetID()) };
			OpenGL::GTexture::bind(id, unit);
		}

		void bind(const Framebuffer& buffer) {
			OpenGL::GFramebuffer::bind(buffer, _framebufferIDs.at(buffer.assetID()));
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

				Vector<TextureID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textureIDs.at(texture.assetID()));
					texture.width(width);
					texture.height(height);
					_textureIDs.erase(texture.assetID());
				}

				OpenGL::GFramebuffer::release(_framebufferIDs.at(assetID), ids);
				_framebufferIDs.erase(assetID);

				buffer.attachments().clear();
				buffer.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
				buffer.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
				
				load(buffer);
			}
		}

		void release(Mesh& mesh) {
			auto it{ _meshArrays.find(mesh.assetID()) };
			if (it != _meshArrays.end()) {
				OpenGL::GMemory::release(it->second);
				_meshArrays.erase(it);
			}
		}

		void release(InstanceGroup& group) {
			auto it{ _instancedArrays.find(group.assetID()) };
			if (it != _instancedArrays.end()) {
				OpenGL::GMemory::release(it->second);
				_instancedArrays.erase(it);
			}
		}

		void release(Shader& shader) {
			auto it{ _shaderIDs.find(shader.assetID()) };
			if (it != _shaderIDs.end()) {
				OpenGL::GShader::releaseProgram(it->second);
				_shaderIDs.erase(it);
			}
		}

		void release(Texture& texture) {
			auto it{ _textureIDs.find(texture.assetID()) };
			if (it != _textureIDs.end()) {
				OpenGL::GTexture::release(it->second);
				_textureIDs.erase(it);
			}
		}

		void release(Framebuffer& buffer) {
			auto it{ _framebufferIDs.find(buffer.assetID()) };
			if (it != _framebufferIDs.end()) {
				Vector<TextureID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_textureIDs.at(texture.assetID()));
				}

				OpenGL::GFramebuffer::release(it->second, ids);
				_framebufferIDs.erase(it);
			}
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::GShader::uniform(id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };

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
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
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

			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::GShader::uniform(id, uniform, static_cast<int>(unit));
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
			for (auto& [assetID, renderArray] : _meshArrays) {
				OpenGL::GMemory::release(renderArray);
			}
			_meshArrays.clear();

			for (auto& [renderID, renderArray] : _instancedArrays) {
				OpenGL::GMemory::release(renderArray);
			}
			_instancedArrays.clear();

			for (auto& [assetID, shader] : _shaderIDs) {
				OpenGL::GShader::releaseProgram(shader);
			}
			_shaderIDs.clear();

			for (auto& [assetID, texture] : _textureIDs) {
				OpenGL::GTexture::release(texture);
			}
			_textureIDs.clear();

			for (auto& [tag, buffer] : _framebufferIDs) {
				OpenGL::GTexture::release(buffer);
			}
			_framebufferIDs.clear();
		}

	};

}
