#pragma once

#include "core/core_types.h"

namespace Byte {

	class RenderMesh {
	protected:
		MeshID _meshID{};
		MaterialID _materialID{};

		bool _render{ true };
		bool _dynamic{ false };
		bool _frustumCulling{ true };

	public:
		RenderMesh() = default;

		RenderMesh(MeshID meshID, MaterialID materialID)
			: _meshID{ meshID }, _materialID{ materialID } {}

		MeshID mesh() const {
			return _meshID;
		}

		void mesh(MeshID id) {
			_meshID = id;
		}

		MaterialID material() const {
			return _materialID;
		}

		void material(MaterialID id) {
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
