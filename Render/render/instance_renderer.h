#pragma once

#include "core/core_types.h"

namespace Byte {

	class InstanceRenderer {
	private:
		AssetID _instanceGroup{};

	public:
		InstanceRenderer(AssetID instanceGroup)
			: _instanceGroup{ instanceGroup } {
		}

		AssetID instanceGroup() const {
			return _instanceGroup;
		}
	};

}