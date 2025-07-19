#include <iostream>

#include "render/renderer.h"
#include "core/window.h"
#include "render/material.h"
#include "render/camera.h"
#include "application/camera_controller.h"

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
	Mesh mesh{ Primitive::quad() };
	Material material{};
	material.color(Vec4{ 0.4f,0.8f,0.1f, 1.0f });
	Shader shader{ "../Render/shader/default.vert","../Render/shader/forward.frag" };
	shader.uniforms().insert("uColor");
	material.shader("default", shader.assetID());

	World world;
	Renderable renderable{ mesh.assetID(),material.assetID() };
	world.createEntity<Renderable, Transform>(std::move(renderable), Transform{});

	repository.mesh(mesh.assetID(), std::move(mesh));
	repository.material(material.assetID(), std::move(material));

	renderer.submit(std::move(shader));

	world.createEntity<Camera, Transform>(Camera{}, Transform{});

	RenderContext context{ world,repository };

	CameraController controller;

	while (!glfwWindowShouldClose(&window.handle())) {
		auto currentTime{ std::chrono::high_resolution_clock::now() };
		float deltaTime{ std::chrono::duration<float>(currentTime - lastTime).count() };
		lastTime = currentTime;

		frameCount++;
		fpsTimer += deltaTime;

		auto [_, cameraTransform] = context.camera();

		controller.update(window, cameraTransform, deltaTime);

		renderer.render(context);

		renderer.update(window);

		if (fpsTimer >= 1.0f) {
			std::cout << "\033[2J\033[1;1H";
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