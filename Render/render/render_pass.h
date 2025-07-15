#pragma once

#include "render_context.h"
#include "render_data.h"

namespace Byte {

	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(RenderData& data, RenderContext& context) = 0;

		virtual void initialize(RenderData& data) {
		}
		
		virtual void terminate(RenderData& data) {
		}
	};

	class DrawPass: public RenderPass {
	public:
		void render(RenderData& data, RenderContext& context) override {

		}
	};

}