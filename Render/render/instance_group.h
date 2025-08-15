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

		bool _render{ true };
		bool _changed{ false };
		bool _dynamic{ false };
		bool _shadow{ true };

	public:
		InstanceGroup(AssetID mesh, AssetID material, Layout&& layout = Layout{ 3,3,4 })
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

		bool render() const {
			return _render;
		}

		void render(bool value) {
			_render = value;
		}

		bool dynamic() const {
			return _dynamic;
		}

		void dynamic(bool value) {
			_dynamic = value;
		}

		bool shadow() const {
			return _shadow;
		}

		void shadow(bool value) {
			_shadow = value;
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
			_changed = true;
		}

		void remove(RenderID key) {
			auto it{ std::find(_keys.begin(), _keys.end(), key) };
			if (it != _keys.end()) {
				std::ptrdiff_t index{ std::distance(_keys.begin(), it) };
				_keys.erase(it);
				size_t stride{ _layout.stride() / sizeof(float) };
				auto start{ _data.begin() + static_cast<size_t>(index) * stride };
				auto end{ _data.begin() + (static_cast<size_t>(index) + 1) * stride };
				_data.erase(start, end);
				_changed = true;
			}
		}

		void submit(RenderID id, Vector<float>&& add) {
			_keys.push_back(id);
			
			_data.insert(_data.end(), std::make_move_iterator(add.begin()), std::make_move_iterator(add.end()));

			_changed = true;
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

			_changed = true;
		}

		void update(RenderID id, const Transform& transform) {
			Vector<float> data{
				transform.position().x, transform.position().y, transform.position().z,
				transform.scale().x, transform.scale().y, transform.scale().z,
				transform.rotation().x, transform.rotation().y, transform.rotation().z, transform.rotation().w
			};

			update(id, std::move(data));
		}

		void update(RenderID id, Vector<float>&& add) {
			auto it{ std::find(_keys.begin(), _keys.end(), id) };
			if (it != _keys.end()) {
				std::ptrdiff_t index{ std::distance(_keys.begin(), it) };
				size_t stride{ _layout.stride() / sizeof(float) };
				size_t offset{ static_cast<size_t>(index) * stride };
				for (size_t i{ 0 }; i < add.size(); ++i) {
					_data[offset + i] = add[i];
				}
				_changed = true;
			}
		}

		void sync() {
			_changed = false;
		}

		bool changed() const {
			return _changed;
		}

		size_t count() const {
			return _keys.size();
		}
	};

}