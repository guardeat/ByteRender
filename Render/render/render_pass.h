#pragma once

#include "query.h"

namespace Byte {

	template<typename _Query = Query>
	class RenderPass {
	public:
		virtual ~RenderPass() = default;

		virtual void render(Query& query) = 0;
	};

}