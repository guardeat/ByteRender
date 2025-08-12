#pragma once

#include <stdexcept>
#include <string>

struct GLFWwindow;
struct GLFWmonitor;

extern "C" {
	GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
	void glfwDestroyWindow(GLFWwindow* window);
	void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);
	void glfwPollEvents();
	int glfwWindowShouldClose(GLFWwindow* window);
	int glfwInit();
}

namespace Byte {

	class Window {
	private:
		using WindowHandler = GLFWwindow;

		WindowHandler* _handler{ nullptr };

	public:
		Window() = default;

		Window(size_t width, size_t height, const std::string& title = "") {
			initialize(width, height, title);
		}

		~Window() {
			terminate();
		}

		void initialize(size_t width, size_t height, const std::string& title = "") {
			glfwInit();
			_handler = glfwCreateWindow(
				static_cast<int>(width),
				static_cast<int>(height),
				title.c_str(),
				nullptr,
				nullptr);
			if (_handler == nullptr) {
				throw std::runtime_error("Failed to create GLFW window");
			}
		}

		void terminate() {
			if (_handler) {
				glfwDestroyWindow(_handler);
				_handler = nullptr;
			}
		}

		WindowHandler& handle() {
			return *_handler;
		}

		const WindowHandler& handle() const {
			return *_handler;
		}

		void pollEvents() {
			glfwPollEvents();
		}

		bool shouldClose() const {
			return static_cast<bool>(glfwWindowShouldClose(_handler));
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
