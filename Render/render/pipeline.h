#pragma once

#include "core/core_types.h"
#include "render_pass.h"

namespace Byte {

	class Pipeline {
	private:
		Vector<UniquePtr<RenderPass>> _passes;

	public:
		Pipeline() = default;

		Pipeline(const Pipeline& pipeline) {
			_passes.reserve(pipeline._passes.size());
			for (const auto& pass : pipeline._passes) {
				_passes.push_back(pass->clone());
			}
		}

		Pipeline(Pipeline&& pipeline) = default;

		Pipeline& operator=(const Pipeline& pipeline) {
			_passes.reserve(pipeline._passes.size());
			for (const auto& pass : pipeline._passes) {
				_passes.push_back(pass->clone());
			}
		}

		Pipeline& operator=(Pipeline&& pipeline) = default;

		void initialize(RenderData& data) {
			for (auto& pass : _passes) {
				pass->initialize(data);
			}
		}

		void terminate(RenderData& data) {
			for (auto& pass : _passes) {
				pass->terminate(data);
			}
		}

		void render(RenderData& data, RenderContext& context) {
			for (auto& pass : _passes) {
				pass->render(data, context);
			}
		}

		template<typename... Passes>
		static Pipeline build() {
			Pipeline pipeline;
			(pipeline._passes.push_back(std::make_unique<Passes>()), ...);

			return pipeline;
		}
	};

}
