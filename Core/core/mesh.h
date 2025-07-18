#pragma once

#include <vector>
#include <cstdint>
#include <utility>

#include "core_types.h"
#include "uid_generator.h"
#include "layout.h"
#include "asset.h"

namespace Byte {

	class Mesh : public Asset {
	private:
		Vector<float> _vertices;
		Vector<uint32_t> _indices;

		Layout _layout;

		bool _dynamic{ false };

	public:
		Mesh(
			Vector<float>&& vertices,
			Vector<uint32_t>&& indices,
			Layout&& layout = { 3, 3, 2 },
			bool dynamic = false,
			Path&& path = ""
			)
			: Asset(std::move(path)),
			_vertices{ std::move(vertices) },
			_indices{ std::move(indices) },
			_layout{ std::move(layout) },
			_dynamic{ dynamic } {
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

		bool dynamic() const {
			return _dynamic;
		}
	};

	struct Primitive {
		static Mesh cube() {
			Vector<float> vertices = {
				-0.5f, -0.5f,  0.5f,  0, 0, 1,  0, 0,
				 0.5f, -0.5f,  0.5f,  0, 0, 1,  1, 0,
				 0.5f,  0.5f,  0.5f,  0, 0, 1,  1, 1,
				-0.5f,  0.5f,  0.5f,  0, 0, 1,  0, 1,
				
				-0.5f, -0.5f, -0.5f,  0, 0, -1, 0, 0,
				 0.5f, -0.5f, -0.5f,  0, 0, -1, 1, 0,
				 0.5f,  0.5f, -0.5f,  0, 0, -1, 1, 1,
				-0.5f,  0.5f, -0.5f,  0, 0, -1, 0, 1,
				
				-0.5f, -0.5f, -0.5f, -1, 0, 0,  0, 0,
				-0.5f, -0.5f,  0.5f, -1, 0, 0,  1, 0,
				-0.5f,  0.5f,  0.5f, -1, 0, 0,  1, 1,
				-0.5f,  0.5f, -0.5f, -1, 0, 0,  0, 1,
				
				 0.5f, -0.5f, -0.5f, 1, 0, 0,  0, 0,
				 0.5f, -0.5f,  0.5f, 1, 0, 0,  1, 0,
				 0.5f,  0.5f,  0.5f, 1, 0, 0,  1, 1,
				 0.5f,  0.5f, -0.5f, 1, 0, 0,  0, 1,
				 
				 -0.5f, 0.5f,  0.5f,  0, 1, 0,  0, 0,
				  0.5f, 0.5f,  0.5f,  0, 1, 0,  1, 0,
				  0.5f, 0.5f, -0.5f,  0, 1, 0,  1, 1,
				 -0.5f, 0.5f, -0.5f,  0, 1, 0,  0, 1,
				 
				 -0.5f, -0.5f,  0.5f, 0, -1, 0, 0, 0,
				  0.5f, -0.5f,  0.5f, 0, -1, 0, 1, 0,
				  0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 1,
				 -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1
			};

			Vector<uint32_t> indices = {
				0, 1, 2, 2, 3, 0,
				4, 6, 5, 6, 4, 7,
				8, 9,10,10,11, 8,
				12,14,13,14,12,15,
				16,17,18,18,19,16,
				20,22,21,22,20,23
			};

			Layout layout{ 3, 3, 2 };

			return Mesh(std::move(vertices), std::move(indices), std::move(layout));
		}

		static Mesh quad() {
			Vector<float> vertices = {
				-1.0f, -1.0f, 0.0f,  0, 0, 1,   0, 0,
				 1.0f, -1.0f, 0.0f,  0, 0, 1,   1, 0,
				 1.0f,  1.0f, 0.0f,  0, 0, 1,   1, 1,
				-1.0f,  1.0f, 0.0f,  0, 0, 1,   0, 1   
			};

			Vector<uint32_t> indices = {
				0, 1, 2,
				2, 3, 0
			};

			Layout layout{ 3, 3, 2 };

			return Mesh(std::move(vertices), std::move(indices), std::move(layout));
		}
	};

}
