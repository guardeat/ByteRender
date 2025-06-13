#pragma once

#include "core_types.h"
#include "material.h"
#include "mesh.h"

namespace Byte {

	class Texture;

	class Repository {
	private:
		Map<MaterialID, Material> _materials;
		Map<MeshID, Mesh> _meshes;
		Map<TextureID, Texture> _textures;

	public:
		Repository() = default;

		Material& material(MaterialID id) {
			return _materials.at(id);
		}

		const Material& material(MaterialID id) const {
			return _materials.at(id);
		}

		void material(MaterialID id, Material&& value) {
			_materials.emplace(id, std::move(value));
		}

		Mesh& mesh(MeshID id) {
			return _meshes.at(id);
		}

		const Mesh& mesh(MeshID id) const {
			return _meshes.at(id);
		}

		void mesh(MeshID id, Mesh&& value) {
			_meshes.emplace(id, std::move(value));
		}

		Texture& texture(TextureID id) {
			return _textures.at(id);
		}

		const Texture& texture(TextureID id) const {
			return _textures.at(id);
		}

		void texture(TextureID id, Texture&& value) {
			_textures.emplace(id, std::move(value));
		}
	};

}
