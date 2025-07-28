#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "core/layout.h"
#include "ecs/ecs.h"
#include "render_types.h"

namespace Byte {

	class RenderBatch : public Asset {
	private:
		Vector<RenderID> _keys;
		Vector<float> _data;
		Layout _layout;

		bool _update{ false };

	public:
		void submit() {

		}

	};

}
