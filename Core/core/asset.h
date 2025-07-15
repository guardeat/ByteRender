#pragma once

#include "core_types.h"
#include "uid_generator.h"

namespace Byte {

	class Asset {
	protected:
		Path _path;
		AssetID _assetID;

	public:
		Asset(Path&& path = "")
			:_path{ std::move(path) } {
			_assetID = UIDGenerator<AssetID>::generate();
		}

		void path(Path&& path) {
			_path = path;
		}

		const Path& path() const {
			return _path;
		}

		AssetID assetID() const {
			return _assetID;
		}
	};

}