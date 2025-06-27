#pragma once

#include "core/core_types.h"
#include "core/mesh.h"
#include "render_types.h"
#include "render_array.h"
#include "instanced_renderer.h"

namespace Byte {

	//Instanced arrays takes mesh vbo from mesh arrays. Owner is mesh arrays.

	class RenderDevice {
	private:
		Map<MeshID, RenderArray> _meshArrays;
		Map<EntityID, RenderArray> _instancedArrays;

	public:
		void submit(Mesh& mesh) {

		}
	};

}
