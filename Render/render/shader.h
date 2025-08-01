#pragma once

#include <cstdint>

#include "core/core_types.h"
#include "material.h"
#include "render_types.h"

namespace Byte {

	class Shader : public Asset {
	private:
		Path _vertex;
		Path _fragment;
		Path _geometry;

		Set<Tag> _uniforms;

		bool _useMaterial{ false };

	public:
		Shader(Path&& vertex, Path&& fragment, Path&& geometry = "")
			:_vertex{ std::move(vertex) }, _fragment{ std::move(fragment) }, _geometry{ std::move(geometry) } {
		}

		const Path& vertex() const {
			return _vertex;
		}

		const Path& fragment() const {
			return _fragment;
		}

		const Path& geometry() const {
			return _geometry;
		}

		Set<Tag>& uniforms() {
			return _uniforms;
		}

		const Set<Tag>& uniforms() const {
			return _uniforms;
		}

		void useMaterial(bool value) {
			_useMaterial = value;
		}

		bool useMaterial() const {
			return _useMaterial;
		}
	};

}