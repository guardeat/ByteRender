#pragma once

#include "../Render/render/texture.h"
#include "../Render/render/material.h"
#include "core_types.h"
#include "mesh.h"

namespace Byte {

	class Repository {
	private:
		Map<MaterialID, Material> _materials;
		Map<MeshID, Mesh> _meshes;
		Map<TextureID, Texture> _textures;

	public:
		Repository() = default;

		Map<MaterialID, Material>& materials() {
			return _materials;
		}

		const Map<MaterialID, Material>& materials() const {
			return _materials;
		}

		Map<MeshID, Mesh>& meshes() {
			return _meshes;
		}

		const Map<MeshID, Mesh>& meshes() const {
			return _meshes;
		}

		Map<TextureID, Texture>& textures() {
			return _textures;
		}

		const Map<TextureID, Texture>& textures() const {
			return _textures;
		}

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
