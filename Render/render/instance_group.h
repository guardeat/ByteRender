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
			: Asset(), _mesh{ mesh }, _material{ material }, _layout{ std::move(layout) } {
		}

		AssetID mesh() const {
			return _mesh;
		}

		AssetID material() const {
			return _material;
		}

		const Layout& layout() const {
			return _layout;
		}

		bool dynamic() const {
			return _dynamic;
		}

		void dynamic(bool value) {
			_dynamic = value;
		}

		const Vector<RenderID>& keys() const {
			return _keys;
		}

		Vector<RenderID>& keys() {
			return _keys;
		}

		const Vector<float>& data() const {
			return _data;
		}

		Vector<float>& data() {
			return _data;
		}

		void clear() {
			_keys.clear();
			_data.clear();
			_update = true;
		}

		void add(RenderID key, const float* instanceData, size_t count) {
			_keys.push_back(key);
			_data.insert(_data.end(), instanceData, instanceData + count);
			_update = true;
		}

		void remove(RenderID key) {
			auto it{ std::find(_keys.begin(), _keys.end(), key) };
			if (it != _keys.end()) {
				std::ptrdiff_t index{ std::distance(_keys.begin(), it) };
				_keys.erase(it);
				size_t stride{ _layout.stride() / sizeof(float) };
				_data.erase(_data.begin() + static_cast<size_t>(index) * stride, _data.begin() + (static_cast<size_t>(index) + 1) * stride);
				_update = true;
			}
		}

		void submit(RenderID id, Vector<float>&& add) {
			_keys.push_back(id);
			
			_data.insert(_data.end(), std::make_move_iterator(add.begin()), std::make_move_iterator(add.end()));

			_update = true;
		}

		void submit(RenderID id, const Transform& transform) {
			_keys.push_back(id);

			_data.push_back(transform.position().x);
			_data.push_back(transform.position().y);
			_data.push_back(transform.position().z);

			_data.push_back(transform.scale().x);
			_data.push_back(transform.scale().y);
			_data.push_back(transform.scale().z);

			_data.push_back(transform.rotation().x);
			_data.push_back(transform.rotation().y);
			_data.push_back(transform.rotation().z);
			_data.push_back(transform.rotation().w);

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