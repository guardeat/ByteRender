#pragma once

#include "core/core_types.h"
#include "core/repository.h"
#include "core/mesh.h"
#include "ecs/ecs.h"
#include "material.h"
#include "texture.h"

namespace Byte {

	class RenderContext {
	private:
		World* _world;
		Repository* _repository;

	public:
		RenderContext(World& world, Repository& repository)
			:_world{ &world }, _repository{&repository} {
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
			_repository->mesh(id);
		}

		const Mesh& mesh(AssetID id) const {
			_repository->mesh(id);
		}

		Material& material(MaterialID id) {
			_repository->material(id);
		}

		const Material& material(MaterialID id) const {
			_repository->material(id);
		}

		Texture& texture(TextureID id) {
			_repository->texture(id);
		}

		const Texture& texture(TextureID id) const {
			_repository->texture(id);
		}
	};

}
