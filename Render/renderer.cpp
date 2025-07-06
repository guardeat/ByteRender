#include "render/renderer.h"

namespace Byte {

	void Renderer::initialize(Window& window) {
		OpenGLAPI::initialize(window);
	}

	void Renderer::render(RenderContext& context) {
	}

	void Renderer::load(Mesh& mesh) {
	}

	void Renderer::load(InstancedRenderable& instanced) {
	}

	void Renderer::update(Window& window) {
		_device.update(window);

		if (_data.width != window.width() || _data.height != window.height()) {
			resize(window.width(), window.height());
		}
	}

	void Renderer::resize(size_t width, size_t height) {

	}

}