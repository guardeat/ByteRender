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

	template<typename Accessor>
	struct GPUEntity {
		Accessor accessor;
		size_t inactiveFrames{};
	};

	class RenderDevice {
	private:
		Map<AssetID, GPUEntity<RenderArray>> _meshArrays;
		Map<AssetID, GPUEntity<RenderArray>> _instancedArrays;
		Map<AssetID, GPUEntity<ShaderID>> _shaderIDs;
		Map<AssetID, GPUEntity<TextureID>> _textureIDs;

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
			RenderArray meshArray{ OpenGL::Memory::buildRenderArray(mesh) };
			_meshArrays.emplace(mesh.assetID(), std::move(meshArray));
		}

		void load(InstanceGroup& instanced, Mesh& mesh) {

		}

		void load(Shader& shader) {
			uint32_t vertex{ OpenGL::Shader::compileShader(shader.vertex(),ShaderType::VERTEX)};
			uint32_t fragment{ OpenGL::Shader::compileShader(shader.fragment(),ShaderType::FRAGMENT)};
			uint32_t geometry{};

			if (!shader.geometry().empty()) {
				geometry = OpenGL::Shader::compileShader(shader.geometry(), ShaderType::GEOMETRY);
			}

			_shaderIDs.emplace(shader.assetID(), OpenGL::Shader::buildProgram(vertex, fragment, geometry));
		}

		void load(Texture& texture) {

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

		void update(Window& window) {
			OpenGL::update(window);

			releaseInactive(_meshArrays, OpenGL::Memory::release);
			releaseInactive(_instancedArrays, OpenGL::Memory::release);
			releaseInactive(_shaderIDs, OpenGL::Shader::releaseProgram);
			releaseInactive(_textureIDs, OpenGL::Texture::release);
		}

		void bind(const Mesh& mesh) {
			GPUEntity<RenderArray>& entity{ _meshArrays.at(mesh.assetID()) };
			entity.inactiveFrames = 0;
			OpenGL::Memory::bind(entity.accessor.id);
		}

		void bind(const Shader& shader) {
			GPUEntity<ShaderID>& entity{ _shaderIDs.at(shader.assetID()) };
			entity.inactiveFrames = 0;
			OpenGL::Shader::bind(entity.accessor);
		}

		void bind(const InstanceGroup& group) {
			GPUEntity<RenderArray>& entity{ _instancedArrays.at(group.assetID()) };
			entity.inactiveFrames = 0;
			OpenGL::Memory::bind(entity.accessor.id);
		}

		void bind(const Texture& texture) {
			GPUEntity<TextureID>& entity{ _textureIDs.at(texture.assetID()) };
			entity.inactiveFrames = 0;
			OpenGL::Texture::bind(entity.accessor);
		}

		void release(Mesh& mesh) {
			auto it{ _meshArrays.find(mesh.assetID()) };
			if (it != _meshArrays.end()) {
				OpenGL::Memory::release(it->second.accessor);
				_meshArrays.erase(it);
			}
		}

		void release(InstanceGroup& group) {
			auto it{ _instancedArrays.find(group.assetID()) };
			if (it != _instancedArrays.end()) {
				OpenGL::Memory::release(it->second.accessor);
				_instancedArrays.erase(it);
			}
		}

		void release(Shader& shader) {
			auto it{ _shaderIDs.find(shader.assetID()) };
			if (it != _shaderIDs.end()) {
				OpenGL::Shader::releaseProgram(it->second.accessor);
				_shaderIDs.erase(it);
			}
		}

		void release(Texture& texture) {
			
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			ShaderID id{ _shaderIDs.at(shader.assetID()).accessor };
			OpenGL::Shader::uniform(id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			ShaderID id{ _shaderIDs.at(shader.assetID()).accessor };

			if (shader.uniforms().contains("uColor")) {
				OpenGL::Shader::uniform(id, "uColor", material.color());
			}

			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &id, &shader](const auto& inputValue) {
						OpenGL::Shader::uniform(id, tag, inputValue);
						}, input);
				}
			}
		}

		void uniform(const Shader& shader, const Transform& transform) {
			ShaderID id{ _shaderIDs.at(shader.assetID()).accessor };
			OpenGL::Shader::uniform(id, "uPosition", transform.position());
			OpenGL::Shader::uniform(id, "uScale", transform.scale());
			OpenGL::Shader::uniform(id, "uRotation", transform.rotation());
		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::Draw::elements(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::Draw::elementsInstanced(size, instanceCount, drawType);
		}

		void state(RenderState state) {
			OpenGL::state(state);
		}

		void clear() {
			for (auto& [assetID, renderArray] : _meshArrays) {
				OpenGL::Memory::release(renderArray.accessor);
			}
			_meshArrays.clear();

			for (auto& [renderID, renderArray] : _instancedArrays) {
				OpenGL::Memory::release(renderArray.accessor);
			}
			_instancedArrays.clear();

			for (auto& [assetID, shader] : _shaderIDs) {
				OpenGL::Shader::releaseProgram(shader.accessor);
			}
			_shaderIDs.clear();

			for (auto& [assetID, texture] : _textureIDs) {
				OpenGL::Texture::release(texture.accessor);
			}
			_textureIDs.clear();
		}

		private:
			template<typename Container, typename ReleaseFunc>
			void releaseInactive(Container& container, ReleaseFunc releaseFunc) {
				std::vector<AssetID> toErase;

				for (auto& [id, entity] : container) {
					entity.inactiveFrames++;
					if (entity.inactiveFrames > _maxInactiveFrames) {
						releaseFunc(entity.accessor);
						toErase.push_back(id);
					}
				}

				for (auto& id : toErase) {
					container.erase(id);
				}
			}

	};

}
