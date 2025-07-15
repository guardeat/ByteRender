#pragma once

#include "core_types.h"
#include "mesh.h"

namespace Byte {

	class Material;
	class Texture;

	class Repository {
	private:
		Map<AssetID, Material> _materials;
		Map<AssetID, Mesh> _meshes;
		Map<AssetID, Texture> _textures;

	public:
		Repository() = default;

		Map<AssetID, Material>& materials() {
			return _materials;
		}

		const Map<AssetID, Material>& materials() const {
			return _materials;
		}

		Map<AssetID, Mesh>& meshes() {
			return _meshes;
		}

		const Map<AssetID, Mesh>& meshes() const {
			return _meshes;
		}

		Map<AssetID, Texture>& textures() {
			return _textures;
		}

		const Map<AssetID, Texture>& textures() const {
			return _textures;
		}

		Material& material(AssetID id) {
			return _materials.at(id);
		}

		const Material& material(AssetID id) const {
			return _materials.at(id);
		}

		void material(AssetID id, Material&& value) {
			_materials.emplace(id, std::move(value));
		}

		Mesh& mesh(AssetID id) {
			return _meshes.at(id);
		}

		const Mesh& mesh(AssetID id) const {
			return _meshes.at(id);
		}

		void mesh(AssetID id, Mesh&& value) {
			_meshes.emplace(id, std::move(value));
		}

		Texture& texture(AssetID id) {
			return _textures.at(id);
		}

		const Texture& texture(AssetID id) const {
			return _textures.at(id);
		}

		void texture(AssetID id, Texture&& value) {
			_textures.emplace(id, std::move(value));
		}
	};

}
