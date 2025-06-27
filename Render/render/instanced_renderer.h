#pragma once

#include "core/core_types.h"
#include "core/layout.h"
#include "mesh_renderer.h"

namespace Byte {

	template<typename Key = EntityID>
	class InstancedRenderer : public MeshRenderer {
	private:
		Vector<Key> _keys;
		Vector<float> _data;
		Layout _layout;

		bool _update{ false };

	public:
		void submit() {

		}

	};

	class RenderInstance {
	private:
		EntityID _instancedRendererID{};
	};

}
