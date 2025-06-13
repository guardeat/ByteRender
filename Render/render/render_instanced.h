#pragma once

#include "core/core_types.h"
#include "core/layout.h"
#include "render_mesh.h"

namespace Byte {
	
	using InstanceID = uint64_t;

	class RenderInstanced : public RenderMesh {
	private:
		Vector<InstanceID> _ids;
		Vector<float> _data;
		Layout _layout;

		bool _update{ false };

	public:
		void submit() {

		}

	};

}
