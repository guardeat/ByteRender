#include "render/renderer.h"

namespace Byte {

	Renderer::~Renderer() {
		clearGPU();
	}

	void Renderer::initialize(Window& window) {
		_data.device.initialize(window);
	}

	void Renderer::render(RenderContext& context) {
		load(context);
		
		_pipeline.render(_data,context);
	}

	void Renderer::load(Mesh& mesh) {
		if (!_data.device.containsMesh(mesh.assetID())) {
			_data.device.load(mesh);
		}
	}

	void Renderer::load(InstancedRenderable& instanced) {
	}

	void Renderer::load(Texture& texture) {
	}

	void Renderer::load(RenderContext& context) {
		for (auto& [_, shader] : _data.shaders) {
			if (!shader.id()) {
				_data.device.load(shader);
			}
		}

		for (auto& [_, mesh] : context.repository().meshes()) {
			load(mesh);
		}

		for (auto& [id, texture] : context.repository().textures()) {
			load(texture);
		}
	}

	void Renderer::submit(Shader&& shader)
	{
		_data.shaders.emplace(shader.assetID(), std::move(shader));
	}

	void Renderer::update(Window& window) {
		_data.device.update(window);

		if (_data.width != window.width() || _data.height != window.height()) {
			resize(window.width(), window.height());
		}
	}

	void Renderer::resize(size_t width, size_t height) {

	}

	void Renderer::clearGPU() {
		for (auto& [_, shader] : _data.shaders) {
			_data.device.release(shader);
		}

		_data.device.clear();
	}

}