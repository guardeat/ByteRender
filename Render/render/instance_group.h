#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "core/layout.h"
#include "ecs/ecs.h"
#include "render_types.h"

namespace Byte {

	class InstanceGroup : public Asset {
	private:
		AssetID _mesh{};
		AssetID _material{};
		Vector<RenderID> _keys;
		Vector<float> _data;
		Layout _layout;

		bool _update{ false };
		bool _dynamic{ false };

	public:
		InstanceGroup(AssetID mesh, AssetID material, Layout&& layout)
			:_mesh{ mesh }, _material{ material }, _layout{ std::move(layout) } {
		}

		AssetID mesh() const {
			return _mesh;
		}

		void submit() {

			_update = true;
		}

		void update() {
			_update = false;
		}

		bool updated() const {
			return _update;
		}
	};

}
