#pragma once

#include "core/repository.h"
#include "ecs/ecs.h"
#include "render/render.h"

namespace Byte {

	class Scene {
	private:
		Repository _repository;
		World _world;
		EntityID _mainCamera;
		EntityID _mainLight;
		AssetID _pointLightGroup{};

	public:
		Scene() {
			_mainCamera = _world.create<Camera, Transform>(Camera{}, Transform{});

			_mainLight = _world.create<DirectionalLight, Transform>(DirectionalLight{}, Transform{});
			_world.get<Transform>(_mainLight).rotate(Vec3{ -45.0f, 0.0f, 0.0f });

			Mesh emptyMesh{ Primitive::sphere(10) };
			AssetID emptyMeshID{ emptyMesh.assetID() };
			_repository.mesh(emptyMesh.assetID(), std::move(emptyMesh));

			InstanceGroup pointLightGroup{ emptyMeshID, 0, Layout{ 3, 3, 3, 3 } };
			pointLightGroup.shadow(false);
			_pointLightGroup = pointLightGroup.assetID();
			_repository.instanceGroup(pointLightGroup.assetID(), std::move(pointLightGroup));
		}

		void update(float dt) {
			updatePointLights();
		}

		RenderContext renderContext() {
			return RenderContext{ _world, _repository, _mainCamera, _mainLight };
		}

		Repository& repository() {
			return _repository;
		}

		const Repository& repository() const {
			return _repository;
		}

		World& world() {
			return _world;
		}

		const World& world() const {
			return _world;
		}

		AssetID pointLightGroup() const {
			return _pointLightGroup;
		}

	private:
		void updatePointLights() {
			InstanceGroup& group{ _repository.instanceGroup(_pointLightGroup) };

			auto view{ _world.components<EntityID, PointLight, Transform>().exclude<InstanceRenderer>() };
			Vector<EntityID> ids;
			for (auto [id, pointLight, transform] : view) {
				group.submit(id, Vector<float>{
					transform.position().x, transform.position().y, transform.position().z,
					transform.scale().x, transform.scale().y, transform.scale().z,
					pointLight.color.x, pointLight.color.y, pointLight.color.z,
					pointLight.constant, pointLight.linear, pointLight.quadratic
				});

				ids.push_back(id);
			}

			for(size_t idx{}; idx < ids.size(); ++idx) {
				InstanceRenderer renderer{ group.assetID() };
				_world.attach(ids[idx], std::move(renderer));
			}
		}

	};

}
