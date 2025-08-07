#pragma once

#include "render_data.h"
#include "render_context.h"

namespace Byte {

	class IRenderPass {
	public:
		virtual ~IRenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}

		virtual void terminate(RenderData& data) {
		}

		virtual UniquePtr<IRenderPass> clone() const = 0;
	};

	template<typename Derived>
	class RenderPass : public IRenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}
		
		virtual void terminate(RenderData& data) {
		}

		UniquePtr<IRenderPass> clone() const override {
			return std::make_unique<Derived>(static_cast<const Derived&>(*this));
		}
	};

}