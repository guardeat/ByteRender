#pragma once

#include "opengl_api.h"
#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "render_types.h"
#include "render_array.h"
#include "instanced_renderable.h"
#include "shader.h"

namespace Byte {

	//TODO: This class covers most basic form. make it more generic. Not just Mesh - RenderArray pair, Mesh - Descriptor? pairs.
	//TODO: Also don't send to gpu immediately. Implement a logic here.

	class RenderDevice {
	private:
		Map<AssetID, RenderArray> _meshArrays;
		Map<AssetID, Shader> _shaders;
		Map<RenderID, RenderArray> _instancedArrays;

	public:
		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void submit(Mesh& mesh) {
			RenderArray meshArray{ OpenGL::Memory::buildRenderArray(mesh) };
			_meshArrays.emplace(mesh.assetID(), meshArray);
		}

		void submit(InstancedRenderable& instanced) {

		}

		void submit(AssetID id, Shader& shader) {

		}

		bool containsMesh(AssetID id) {
			return _meshArrays.contains(id);
		}

		bool containsInstanced(RenderID id) {
			return _instancedArrays.contains(id);
		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void bindShader(const Shader& shader) {
			OpenGL::Shader::bind(shader.id);
		}

		const Shader& shader(AssetID id) {
			return _shaders.at(id);
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			OpenGL::Shader::uniform(shader.id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			bindShader(shader);
			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms.contains(tag)) {
					std::visit([this, &tag, &shader](const auto& inputValue) {
						OpenGL::Shader::uniform(shader.id, tag, inputValue);
						}, input);
				}
			}
		}
	};

}
