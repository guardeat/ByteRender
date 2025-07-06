#pragma once

#include "opengl_api.h"
#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "render_types.h"
#include "render_array.h"
#include "instanced_renderable.h"

namespace Byte {

	class RenderDevice {
	private:
		Map<MeshID, RenderArray> _meshArrays;
		Map<EntityID, RenderArray> _instancedArrays;

	public:
		void initialize(Window& window) {
			OpenGLAPI::initialize(window);
		}

		void submit(Mesh& mesh) {

		}

		void submit(InstancedRenderable& instanced) {

		}

		void update(Window& window) {
			OpenGLAPI::update(window);
		}
	};

}
