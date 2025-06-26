#pragma once

#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>

namespace Byte {

	class Window {
	private:
		using WindowHandler = GLFWwindow;

		WindowHandler* _handler{ nullptr };

	public:
		Window(size_t width, size_t height, const std::string& title) {
			_handler = glfwCreateWindow(
				static_cast<int>(width),
				static_cast<int>(height),
				title.c_str(),
				nullptr,
				nullptr);
			if (_handler == nullptr) {
				throw std::exception("Failed to create GLFW window");
			}
		}

		~Window() {
			glfwDestroyWindow(_handler);
		}

		WindowHandler& handle() {
			return *_handler;
		}

		const WindowHandler& handle() const {
			return *_handler;
		}

		size_t width() const {
			int width, height;
			glfwGetWindowSize(_handler, &width, &height);

			return static_cast<size_t>(width);
		}

		size_t height() const {
			int width, height;
			glfwGetWindowSize(_handler, &width, &height);

			return static_cast<size_t>(height);
		}

	};

}
