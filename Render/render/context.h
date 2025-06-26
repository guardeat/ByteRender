#pragma once

#include "core/core_types.h"
#include "core/repository.h"
#include "core/mesh.h"
#include "core/material.h"
#include "ecs/ecs.h"
#include "texture.h"

namespace Byte {

	class Context {
	private:
		World* _world;
		Repository* _repository;

	public:
		Context(World& world, Repository& repository)
			:_world{ &world }, _repository{&repository} {
		}

		template<typename... Args>
		auto view() {
			return _world->components<Args...>();
		}

		Mesh& mesh(MeshID id) {
			_repository->mesh(id);
		}

		Material& material(MaterialID id) {
			_repository->material(id);
		}

		Texture& texture(TextureID id) {
			_repository->texture(id);
		}
	};

}
