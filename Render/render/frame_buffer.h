#pragma once

#include "core/core_types.h"
#include "render_types.h"
#include "texture.h"

namespace Byte {

	class Framebuffer {
	private:
		FramebufferID _id{};
		Vector<Texture> _attachments;

	public:
		FramebufferID id() const {
			return _id;
		}

		void id(FramebufferID newID) {
			_id = newID;
		}
	};

}
