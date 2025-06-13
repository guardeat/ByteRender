#pragma once

#include "math/vec.h"
#include "math/quaternion.h"
#include "math/mat.h"

namespace Byte {

	struct TransformData {
		Vec3 position{};
		Vec3 scale{ 1.0f, 1.0f, 1.0f };
		Quaternion rotation{};
	};

	class Transform {
	private:
		TransformData _local;
		TransformData _global;

		bool _changed{ true };

	public:
		const Vec3& position() const { 
			return _global.position; 
		}

		const Vec3& scale() const {
			return _global.scale; 
		}

		const Quaternion& rotation() const { 
			return _global.rotation; 
		}

		void position(const Vec3& pos) {
			Vec3 delta = pos - _global.position;
			_local.position += delta;
			_global.position = pos;
			_changed = true;
		}

		void scale(const Vec3& scale) {
			Vec3 ratio = scale / _global.scale;
			_local.scale *= ratio;
			_global.scale = scale;
			_changed = true;
		}

		void rotation(const Quaternion& rot) {
			_local.rotation = rot;
			_global.rotation = rot;
			_local.rotation.normalize();
			_global.rotation.normalize();
			_changed = true;
		}

		void rotation(const Vec3& euler) {
			rotation(Quaternion{ euler });
		}

		void rotate(const Quaternion& delta) {
			_local.rotation = delta * _local.rotation;
			_global.rotation = delta * _global.rotation;
			_local.rotation.normalize();
			_global.rotation.normalize();
			_changed = true;
		}

		void rotate(const Vec3& euler) {
			rotate(Quaternion{ euler });
		}

		Vec3 front() const { 
			return _global.rotation * Vec3{ 0, 0, -1 }; 
		}

		Vec3 up() const { 
			return _global.rotation * Vec3{ 0, 1, 0 }; 
		}

		Vec3 right() const { 
			return _global.rotation * Vec3{ 1, 0, 0 }; 
		}

		Mat4 view() const {
			Vec3 f = front().normalized();
			Vec3 r = right().normalized();
			Vec3 u = r.cross(f);
			return Mat4::view(_global.position, _global.position + f, u);
		}

		bool changed() const {
			return _changed;
		}

		const TransformData& local() const {
			return _local;
		}

		const TransformData& global() const {
			return _global;
		}
	};

}
