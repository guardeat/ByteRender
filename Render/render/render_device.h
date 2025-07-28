#pragma once

#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/transform.h"
#include "opengl_api.h"
#include "render_types.h"
#include "render_array.h"
#include "render_batch.h"
#include "shader.h"

namespace Byte {

	class RenderDevice {
	private:
		Map<AssetID, RenderArray> _meshArrays;
		Map<AssetID, ShaderID> _shaderIDs;
		Map<AssetID, TextureID> _textureIDs;
		Map<AssetID, RenderArray> _instancedArrays;

	public:
		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void load(Mesh& mesh) {
			RenderArray meshArray{ OpenGL::Memory::buildRenderArray(mesh) };
			_meshArrays.emplace(mesh.assetID(), meshArray);
		}

		void load(RenderBatch& instanced) {

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

		bool loaded(Shader& shader) {
			return _shaderIDs.contains(shader.assetID());
		}

		bool loaded(Texture& texture) {
			return _textureIDs.contains(texture.assetID());
		}

		void release(Mesh& mesh) {
			OpenGL::Memory::release(_meshArrays.at(mesh.assetID()));
			_meshArrays.erase(mesh.assetID());
		}

		void release(Shader& shader) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::Shader::releaseProgram(id);
		}

		void release(Texture& texture) {

		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void bind(const Mesh& mesh) {
			RenderArray renderArray{ _meshArrays.at(mesh.assetID()) };
			OpenGL::Memory::bind(renderArray.id);
		}

		void bind(const Shader& shader) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::Shader::bind(id);
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
			OpenGL::Shader::uniform(id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			ShaderID id{ _shaderIDs.at(shader.assetID()) };

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
			ShaderID id{ _shaderIDs.at(shader.assetID()) };
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
				OpenGL::Memory::release(renderArray);
			}
			_meshArrays.clear();

			for (auto& [renderID, renderArray] : _instancedArrays) {
				OpenGL::Memory::release(renderArray);
			}
			_instancedArrays.clear();

			for (auto& [assetID, shaderID] : _shaderIDs) {
				OpenGL::Shader::releaseProgram(shaderID);
			}
			_shaderIDs.clear();

			for (auto& [assetID, textureID] : _textureIDs) {
				//TODO:
			}
			_textureIDs.clear();
		}

	};

}
