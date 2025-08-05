#include <iostream>
#include <chrono>

#include "core/window.h"
#include "application/camera_controller.h"

#include "render/renderer.h"
#include "render/material.h"
#include "render/camera.h"
#include "render/light.h"
#include "render/instance_renderer.h"

using namespace Byte;

// Prefer discrete GPU on laptops
extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main() {
	glfwInit();

	Window window{ 1280, 720 };
	Renderer renderer{ Renderer::build<SkyboxPass, ShadowPass, GeometryPass, LightingPass, DrawPass>() };

	Repository repository{};
	World world{};

	auto lastTime{ std::chrono::high_resolution_clock::now() };
	int frameCount{ 0 };
	float fpsTimer{ 0.0f };

	// === Sphere Setup ===
	Mesh sphereMesh{ Primitive::sphere(10) };
	Material sphereMaterial{};
	sphereMaterial.color({ 0.9f, 0.1f, 0.2f, 1.0f });

	InstanceGroup sphereGroup{ sphereMesh.assetID(), sphereMaterial.assetID() };
	repository.mesh(sphereMesh.assetID(), std::move(sphereMesh));
	repository.material(sphereMaterial.assetID(), std::move(sphereMaterial));

	// === Ground Setup ===
	Mesh groundMesh{ Primitive::cube() };
	Material groundMaterial{};
	groundMaterial.color({ 0.2f, 0.9f, 0.3f, 1.0f });

	MeshRenderer groundRenderer{ groundMesh.assetID(), groundMaterial.assetID() };
	EntityID groundEntity{ world.create<MeshRenderer, Transform>(std::move(groundRenderer), Transform{}) };
	world.get<Transform>(groundEntity).position({ 0.0f, -1.0f, 0.0f });
	world.get<Transform>(groundEntity).scale({ 200.0f, 1.0f, 200.05f });

	repository.mesh(groundMesh.assetID(), std::move(groundMesh));
	repository.material(groundMaterial.assetID(), std::move(groundMaterial));

	// === Point Light Setup ===
	Mesh pointLightMesh{ Primitive::sphere(10) };
	InstanceGroup pointLightGroup{ pointLightMesh.assetID(), 0, Layout{ 3, 3, 3, 3 } };

	for (size_t i{ 0 }; i < 10; ++i) {
		for (size_t j{ 0 }; j < 10; ++j) {
			for (size_t k{ 0 }; k < 10; ++k) {
				Vec3 position{ static_cast<float>(i), static_cast<float>(j), static_cast<float>(k) };

				InstanceRenderer rendererComponent{ sphereGroup.assetID() };
				EntityID visualEntity{ world.create<InstanceRenderer, Transform>(std::move(rendererComponent), Transform{}) };
				auto& visualTransform{ world.get<Transform>(visualEntity) };
				visualTransform.position(position);
				sphereGroup.submit(visualEntity, visualTransform);
			}
		}
	}

	repository.instanceGroup(sphereGroup.assetID(), std::move(sphereGroup));

	renderer.parameter("point_light_group_id", pointLightGroup.assetID());
	repository.instanceGroup(pointLightGroup.assetID(), std::move(pointLightGroup));
	repository.mesh(pointLightMesh.assetID(), std::move(pointLightMesh));

	// === Skybox ===
	Material skybox{};
	skybox.parameter("uScatter", Vec3{ 0.1f, 0.2f, 0.9f });
	renderer.parameter("skybox_material", skybox.assetID());
	repository.material(skybox.assetID(), std::move(skybox));

	// === Camera & Light ===
	EntityID camera{ world.create<Camera, Transform>(Camera{}, Transform{}) };
	EntityID dirLight{ world.create<DirectionalLight, Transform>(DirectionalLight{}, Transform{}) };
	world.get<Transform>(dirLight).rotate(Vec3{ -45.0f, 0.0f, 0.0f });

	RenderContext context{ world, repository, camera, dirLight };
	CameraController controller{};

	// === Renderer Setup ===
	renderer.parameter("default_shader_path", Path{ "../Render/shader/" });
	renderer.initialize(window);

	std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << "\n";
	std::cout << "Version: " << glGetString(GL_VERSION) << "\n";

	// === Main Loop ===
	while (!glfwWindowShouldClose(&window.handle())) {
		auto currentTime{ std::chrono::high_resolution_clock::now() };
		float deltaTime{ std::chrono::duration<float>(currentTime - lastTime).count() };
		lastTime = currentTime;

		++frameCount;
		fpsTimer += deltaTime;

		auto [_, camTransform] = context.camera();
		controller.update(window, camTransform, deltaTime);

		renderer.render(context);
		renderer.update(window);
		glfwPollEvents();

		if (fpsTimer >= 1.0f) {
			std::cout << "\033[2J\033[1;1H";
			std::cout << "FPS: " << frameCount << "\n";

			GLenum error{ glGetError() };
			if (error) {
				std::cout << "GRAPHIC ERROR: " << error << "\n";
			}
			frameCount = 0;
			fpsTimer = 0.0f;
		}
	}

	return 0;
}
