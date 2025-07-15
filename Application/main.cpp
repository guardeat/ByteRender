#include "render/renderer.h"
#include "core/window.h"
#include "render/material.h"

using namespace Byte;

int main() {
	glfwInit();

	Window window{ 1280,720 };

	Renderer renderer{ Renderer::build<DrawPass>() };

	renderer.initialize(window);

	auto lastTime{ std::chrono::high_resolution_clock::now() };
	int frameCount{ 0 };
	float fpsTimer{ 0.0f };

	Repository repository;
	Mesh m{ Primitive::cube() };
	repository.mesh(m.id(), std::move(m));

	World world;

	RenderContext context{ world,repository };

	while (!glfwWindowShouldClose(&window.handle())) {
		auto currentTime{ std::chrono::high_resolution_clock::now() };
		float deltaTime{ std::chrono::duration<float>(currentTime - lastTime).count() };
		lastTime = currentTime;

		frameCount++;
		fpsTimer += deltaTime;

		renderer.render(context);

		renderer.update(window);

		if (fpsTimer >= 1.0f) {
			std::cout << "FPS: " << frameCount << std::endl;
			GLenum error{ glGetError() };
			if (error) {
				std::cout << "GRAPHIC ERROR: " << error << std::endl;
			}
			frameCount = 0;
			fpsTimer = 0.0f;
		}

		glfwPollEvents();
	}

	return 0;
}