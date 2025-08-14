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
		using GPUMemoryDevice = GPUMemoryDevice<
			OpenGL,
			GPUResource<Mesh>,
			GPUResource<InstanceGroup>,
			GPUResource<Texture>,
			GPUResource<Shader>,
			GPUResource<Framebuffer>>;

		using GPUUniformDevice = GPUUniformDevice<OpenGL, GPUMemoryDevice>;

		GPUMemoryDevice _memory;
		GPUUniformDevice _uniform;

	public:
		RenderDevice()
			:_uniform{ _memory } {
		}

		RenderDevice(const RenderDevice& left) = delete;

		RenderDevice(RenderDevice&& right) noexcept
			:_memory{ std::move(right._memory) }, _uniform{right._memory} {
		}

		RenderDevice& operator=(const RenderDevice& left) = delete;

		RenderDevice& operator=(RenderDevice&& right) {
			_memory.clear();
			_memory = std::move(right._memory);

			_uniform.memory(_memory);
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

		GPUUniformDevice& uniform() {
			return _uniform;
		}

		const GPUUniformDevice& uniform() const {
			return _uniform;
		}

		void update(Window& window) {
			OpenGL::update(window);
		}

		void viewport(size_t width, size_t height) {
			OpenGL::viewport(width, height);
		}

		void blendWeights(float source, float destination) {
			OpenGL::blendWeights(source, destination);
		}

		void clearBuffer() {
			OpenGL::clear();
		}

		void resize(Framebuffer& buffer, size_t width, size_t height) {
			if (buffer.resize()) {
				AssetID assetID{ buffer.assetID() };

				Vector<GPUResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_memory.get(texture).id);

					texture.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
					texture.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
					_memory.data().erase(texture.assetID());
				}

				OpenGL::release(_memory.get(buffer), ids);
				_memory.data().erase(assetID);

				buffer.attachments().clear();
				buffer.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
				buffer.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));

				_memory.load(buffer);
			}
		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::draw(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			OpenGL::draw(size, instanceCount, drawType);
		}

		void state(RenderState state) {
			OpenGL::state(state);
		}

		void clear() {
			_memory.clear();
		}

	};

}
