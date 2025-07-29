#pragma once

#include "core/core_types.h"
#include "core/asset.h"
#include "render_types.h"
#include "texture.h"

namespace Byte {

	class Framebuffer : public Asset {
	private:
		Map<Tag, Texture> _textures;
		Vector<AttachmentType> _attachments;

		size_t _width{};
		size_t _height{};

		bool _resize{ true };

		float _resizeFactor{ 1.0f };

	public:
		Framebuffer(size_t width, size_t height)
			: _width{ width }, _height{height} {
		}

		void texture(Tag&& tag, Texture&& texture) {
			_textures.emplace(std::move(tag), std::move(texture));
		}

		Texture& texture(const Tag& tag) {
			return _textures.at(tag);
		}

		const Texture& texture(const Tag& tag) const {
			return _textures.at(tag);
		}

		Map<Tag, Texture>& textures() {
			return _textures;
		}

		const Map<Tag, Texture>& textures() const {
			return _textures;
		}

		Vector<AttachmentType>& attachments() {
			return _attachments;
		}

		const Vector<AttachmentType>& attachments() const {
			return _attachments;
		}

		void width(size_t newWidth) {
			_width = newWidth;
		}

		void height(size_t newHeight) {
			_height = newHeight;
		}

		size_t width() const {
			return _width;
		}

		size_t height() const {
			return _height;
		}

		bool resize() const {
			return _resize;
		}

		void resize(bool newResize) {
			_resize = newResize;
		}

		float resizeFactor() const {
			return _resizeFactor;
		}

		void resizeFactor(float newResizeFactor) {
			_resizeFactor = newResizeFactor;
		}
	};

}
