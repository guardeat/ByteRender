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
		Map<RenderID, RenderArray> _instancedArrays;

	public:
		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		void load(Mesh& mesh) {
			RenderArray meshArray{ OpenGL::Memory::buildRenderArray(mesh) };
			_meshArrays.emplace(mesh.assetID(), meshArray);
		}

		void load(InstancedRenderable& instanced) {

		}

		void load(Shader& shader) {
			uint32_t vertex{ OpenGL::Shader::compileShader(shader.vertex(),ShaderType::VERTEX)};
			uint32_t fragment{ OpenGL::Shader::compileShader(shader.fragment(),ShaderType::FRAGMENT)};
			uint32_t geometry{};

			if (!shader.geometry().empty()) {
				geometry = OpenGL::Shader::compileShader(shader.geometry(), ShaderType::GEOMETRY);
			}

			shader.id(OpenGL::Shader::buildProgram(vertex, fragment, geometry));
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

		void bind(const Mesh& mesh) {
			RenderArray renderArray{ _meshArrays.at(mesh.assetID()) };
			OpenGL::Memory::bind(renderArray.id);
		}

		void bind(const Shader& shader) {
			OpenGL::Shader::bind(shader.id());
		}

		template<typename Type>
		void uniform(const Shader& shader, const Tag& tag, const Type& value) {
			OpenGL::Shader::uniform(shader.id, tag, value);
		}

		void uniform(const Shader& shader, Material& material) {
			bind(shader);
			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &shader](const auto& inputValue) {
						OpenGL::Shader::uniform(shader.id(), tag, inputValue);
						}, input);
				}
			}
		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::Draw::elements(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::Draw::elementsInstanced(size, instanceCount, drawType);
		}

	};

}
