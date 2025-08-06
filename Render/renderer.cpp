#include "render/renderer.h"

namespace Byte {

	Renderer::~Renderer() {
		clearMemory();
	}

	void Renderer::initialize(Window& window) {
		_data.width = window.width();
		_data.height = window.height();

		_data.device.initialize(window);
		_pipeline.initialize(_data);
	}

	void Renderer::render(RenderContext& context) {
		load(context);
		
		_pipeline.render(_data,context);
	}

	void Renderer::load(RenderContext& context) {
		for (auto& [_, shader] : _data.shaders) {
			if (!_data.device.loaded(shader)) {
				_data.device.load(shader);
			}
		}

		for (auto& [_, buffer] : _data.framebuffers) {
			if (!_data.device.loaded(buffer)) {
				_data.device.load(buffer);
			}
		}

		for (auto& [_, mesh] : _data.meshes) {
			if(!_data.device.loaded(mesh)) {
				_data.device.load(mesh);
			}
		}

		for (auto& [_, mesh] : context.repository().meshes()) {
			if (!_data.device.loaded(mesh)) {
				_data.device.load(mesh);
			}
		}

		for (auto& [id, texture] : context.repository().textures()) {
			if (!_data.device.loaded(texture)) {
				_data.device.load(texture);
			}
		}

		for (auto& [id, instanceGroup] : context.repository().instanceGroups()) {
			if (!_data.device.loaded(instanceGroup)) {
				_data.device.load(instanceGroup,context.repository().mesh(instanceGroup.mesh()));
			}

			else if (instanceGroup.updated()) {
				_data.device.updateBuffer(instanceGroup);
			}
		}
	}

	void Renderer::release(Mesh& mesh) {
		_data.device.release(mesh);
	}

	void Renderer::release(InstanceGroup& group) {
		_data.device.release(group);
	}

	void Renderer::release(Shader& shader) {
		_data.device.release(shader);
		_data.shaders.erase(shader.assetID());
	}

	void Renderer::release(Texture& texture) {
		_data.device.release(texture);
	}

	void Renderer::submit(Shader&& shader) {
		_data.shaders.emplace(shader.assetID(), std::move(shader));
	}

	void Renderer::update(Window& window) {
		_data.device.update(window);

		if (_data.width != window.width() || _data.height != window.height()) {
			resize(window.width(), window.height());
		}
	}

	void Renderer::resize(size_t width, size_t height) {
		_data.width = width;
		_data.height = height;

		for (auto& [id, buffer] : _data.framebuffers) {
			_data.device.resize(buffer, width, height);
		}
	}

	void Renderer::clearMemory() {
		for (auto& [_, shader] : _data.shaders) {
			_data.device.release(shader);
		}

		for(auto& [_, buffer] : _data.framebuffers) {
			_data.device.release(buffer);
		}

		for (auto& [_, mesh] : _data.meshes) {
			_data.device.release(mesh);
		}

		_data.device.clear();
	}

}