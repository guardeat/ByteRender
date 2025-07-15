#pragma once

#include "core/core_types.h"
#include "core/layout.h"
#include "ecs/ecs.h"
#include "renderable.h"

namespace Byte {

	class InstancedRenderable : public Renderable {
	private:
		Vector<RenderID> _keys;
		Vector<float> _data;
		Layout _layout;

		bool _update{ false };

	public:
		void submit() {

		}

	};

	class RenderInstance {
	private:
		EntityID _renderableID{};
	};

}
