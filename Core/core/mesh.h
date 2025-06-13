#pragma once

#include <vector>
#include <cstdint>
#include <utility>

#include "core_types.h"
#include "uid_generator.h"
#include "layout.h"

namespace Byte {

	class Mesh {
	private:
		MeshID _id{};

		Vector<float> _vertices;
		Vector<uint32_t> _indices;

		Layout _layout;

	public:
		Mesh(
			Vector<float>&& vertices,
			Vector<uint32_t>&& indices,
			Layout&& layout = { 3, 3, 2 },
			MeshID id = 0
		)
			: _vertices{ std::move(vertices) },
			_indices{ std::move(indices) },
			_layout{ std::move(layout) },
			_id{id} {
			if (_id == 0) {
				_id = UIDGenerator::generate();
			}
		}

		MeshID id() const {
			return _id;
		}

		const Vector<float>& vertices() const {
			return _vertices;
		}

		const Vector<uint32_t>& indices() const {
			return _indices;
		}

		const Layout& layout() const {
			return _layout;
		}

		size_t vertexCount() const {
			return _vertices.size() / _layout.stride();
		}

		size_t indexCount() const {
			return _indices.size();
		}
	};

}
