#pragma once

#include "core/core_types.h"

namespace Byte {

	class MeshRenderer {
	protected:
		AssetID _meshID{};
		AssetID _materialID{};

		bool _render{ true };
		bool _dynamic{ false };
		bool _frustumCulling{ true };

	public:
		MeshRenderer() = default;

		MeshRenderer(AssetID meshID, AssetID materialID)
			: _meshID{ meshID }, _materialID{ materialID } {}

		AssetID mesh() const {
			return _meshID;
		}

		void mesh(AssetID id) {
			_meshID = id;
		}

		AssetID material() const {
			return _materialID;
		}

		void material(AssetID id) {
			_materialID = id;
		}

		bool render() const {
			return _render;
		}

		void render(bool value) {
			_render = value;
		}

		bool dynamic() const {
			return _dynamic;
		}

		void dynamic(bool value) {
			_dynamic = value;
		}

		bool frustumCulling() const {
			return _frustumCulling;
		}

		void frustumCulling(bool value) {
			_frustumCulling = value;
		}
	};

}
