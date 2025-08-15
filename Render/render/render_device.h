#pragma once

#include "core/core_types.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/transform.h"
#include "opengl_api.h"
#include "render_types.h"
#include "instance_group.h"
#include "shader.h"
#include "texture.h"
#include "device_common.h"

namespace Byte {

	class RenderDevice {
	private:
		using GPUMemoryDevice = GPUMemoryDevice<OpenGL, GPUResource>;

		using GPUShaderDevice = GPUShaderDevice<OpenGL, GPUResource, GPUMemoryDevice>;

		using GPUFramebufferDevice = GPUFramebufferDevice<OpenGL, GPUResource, GPUMemoryDevice>;

		GPUMemoryDevice _memory;
		GPUShaderDevice _shader;
		GPUFramebufferDevice _framebuffer;

	public:
		RenderDevice()
			:_shader{ _memory }, _framebuffer{ _memory } {
		}

		RenderDevice(const RenderDevice& left) = delete;

		RenderDevice(RenderDevice&& right) noexcept
			:_memory{ std::move(right._memory) },
			_shader{ std::move(right._shader) },
			_framebuffer{ std::move(right._framebuffer) } {
			_shader.memory(_memory);
			_framebuffer.memory(_memory);
		}

		RenderDevice& operator=(const RenderDevice& left) = delete;

		RenderDevice& operator=(RenderDevice&& right) noexcept {
			_memory = std::move(right._memory);
			_shader = std::move(right._shader);
			_framebuffer = std::move(right._framebuffer);

			_shader.memory(_memory);
			_framebuffer.memory(_memory);

			return *this;
		}

		~RenderDevice() {
			clear();
		}

		void initialize(Window& window) {
			OpenGL::initialize(window);
		}

		GPUMemoryDevice& memory() {
			return _memory;
		}

		const GPUMemoryDevice& memory() const {
			return _memory;
		}

		GPUShaderDevice& shader() {
			return _shader;
		}

		const GPUShaderDevice& shader() const {
			return _shader;
		}

		GPUFramebufferDevice& framebuffer() {
			return _framebuffer;
		}

		const GPUFramebufferDevice& framebuffer() const {
			return _framebuffer;
		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void blendWeights(float source, float destination) {
			OpenGL::blendWeights(source, destination);
		}

		void state(RenderState state) {
			OpenGL::state(state);
		}

		void clear() {
			_memory.clear();
		}

	};

}
