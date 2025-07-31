#pragma once

#include "core/core_types.h"
#include "core/repository.h"
#include "core/mesh.h"
#include "ecs/ecs.h"
#include "material.h"
#include "texture.h"
#include "camera.h"
#include "light.h"

namespace Byte {

	class RenderContext {
	private:
		World* _world;
		Repository* _repository;

		EntityID _camera;
		EntityID _directionalLight;

	public:
		RenderContext(World& world, Repository& repository, EntityID camera, EntityID dLight)
			:_world{ &world }, _repository{ &repository }, _camera{ camera }, _directionalLight{dLight} {
		}

		Repository& repository() {
			return *_repository;
		}

		template<typename... Args>
		auto view() {
			return _world->components<Args...>();
		}

		template<typename Component>
		Component& get(EntityID id) {
			return _world->get<Component>(id);
		}

		template<typename Component>
		const Component& get(EntityID id) const {
			return _world->get<Component>(id);
		}

		Mesh& mesh(AssetID id) {
			return _repository->mesh(id);
		}

		const Mesh& mesh(AssetID id) const {
			return _repository->mesh(id);
		}

		Material& material(AssetID id) {
			return _repository->material(id);
		}

		const Material& material(AssetID id) const {
			return _repository->material(id);
		}

		Texture& texture(AssetID id) {
			return _repository->texture(id);
		}

		const Texture& texture(AssetID id) const {
			return _repository->texture(id);
		}

		Pair<Camera&, Transform&> camera() {
			Camera& camera{ _world->get<Camera>(_camera) };
			Transform& transform{ _world->get<Transform>(_camera) };
			return Pair<Camera&, Transform&>{camera, transform};
		}

		Pair<DirectionalLight&, Transform&> directionalLight() {
			DirectionalLight& dLight{ _world->get<DirectionalLight>(_directionalLight) };
			Transform& transform{ _world->get<Transform>(_directionalLight) };
			return Pair<DirectionalLight&, Transform&>{dLight, transform};
		}
	};

}
