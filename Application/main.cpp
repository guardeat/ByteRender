#include <iostream>

#include "render/renderer.h"
#include "core/window.h"
#include "render/material.h"
#include "render/camera.h"
#include "render/light.h"
#include "render/instance_renderer.h"
#include "application/camera_controller.h"

using namespace Byte;

extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main() {
	glfwInit();

	Window window{ 1280,720 };

	Renderer renderer{ Renderer::build<SkyboxPass, GeometryPass, LightingPass, DrawPass>() };

	auto lastTime{ std::chrono::high_resolution_clock::now() };
	int frameCount{ 0 };
	float fpsTimer{ 0.0f };

	Repository repository;
	Mesh mesh{ Primitive::sphere(10) };
	Material material{};
	material.color(Vec4{ 0.4f,0.8f,0.1f, 1.0f });
	Shader shader{ "../Render/shader/default.vert","../Render/shader/forward.frag" };
	shader.uniforms().insert("uColor");
	material.shader("default", shader.assetID());

	World world;
	Mesh cubeMesh{ Primitive::cube() };
	MeshRenderer meshRenderer{ cubeMesh.assetID(),material.assetID() };

	EntityID cubeEntity{ world.create<MeshRenderer, Transform>(std::move(meshRenderer), Transform{}) };

	world.get<Transform>(cubeEntity).position(Vec3{ 0.0f, -1.0f, 0.0f });

	repository.mesh(cubeMesh.assetID(), std::move(cubeMesh));
	world.get<Transform>(cubeEntity).scale(Vec3{ 100.0f, 1.0f, 100.05f });

	InstanceGroup instanceGroup{ mesh.assetID(), material.assetID() };

	for (size_t i{}; i < 10; ++i) {
		for(size_t j{}; j < 10; ++j) {
			for(size_t k{}; k < 10; ++k) {
				InstanceRenderer instanceRenderer{ instanceGroup.assetID() };
				EntityID entity{ world.create<InstanceRenderer, Transform>(std::move(instanceRenderer), Transform{}) };
				auto& transform{ world.get<Transform>(entity) };
				transform.position(Vec3{ static_cast<float>(i), static_cast<float>(j), static_cast<float>(k) });
				instanceGroup.submit(entity,transform);
			}
		}
	}

	Mesh pointLightMesh{ Primitive::sphere(10) };
	InstanceGroup pointLightGroup{ pointLightMesh.assetID(), 0, Layout{3,3,3,3} };

	for (size_t i{}; i < 10; ++i) {
		for (size_t j{}; j < 10; ++j) {
			for (size_t k{}; k < 10; ++k) {
				EntityID entity{ world.create<PointLight, Transform>(PointLight{}, Transform{}) };
				auto& transform{ world.get<Transform>(entity) };
				transform.position(Vec3{ static_cast<float>(i), static_cast<float>(j), static_cast<float>(k) } * 10);
				PointLight& light{ world.get<PointLight>(entity) };
				light.color = Vec3{ 1.0f, 0.0f, 0.0f };
				transform.scale(transform.scale() * light.radius());
				Vector<float> data{
					transform.position().x, transform.position().y, transform.position().z,
					transform.scale().x, transform.scale().y, transform.scale().z,
					light.color.x, light.color.y, light.color.z,
					light.constant, light.linear, light.quadratic
				};
				pointLightGroup.submit(entity, std::move(data));
			}
		}
	}

	renderer.parameter("point_light_group_id", pointLightGroup.assetID());

	repository.instanceGroup(pointLightGroup.assetID(), std::move(pointLightGroup));
	repository.instanceGroup(instanceGroup.assetID(), std::move(instanceGroup));

	repository.mesh(pointLightMesh.assetID(), std::move(pointLightMesh));
	repository.mesh(mesh.assetID(), std::move(mesh));
	repository.material(material.assetID(), std::move(material));
	
	Material skybox{};
	skybox.parameter("uScatter", Vec3{ 0.1f,0.2f,0.9f });
	renderer.parameter("skybox_material", skybox.assetID());
	repository.material(skybox.assetID(), std::move(skybox));

	renderer.submit(std::move(shader));

	EntityID camera{ world.create<Camera, Transform>(Camera{}, Transform{}) };
	EntityID dLight{ world.create<DirectionalLight, Transform>(DirectionalLight{}, Transform{}) };

	RenderContext context{ world,repository, camera, dLight };

	CameraController controller;

	renderer.initialize(window);

	std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
	std::cout << "Vendor: " << glGetString(GL_VENDOR) << "\n";
	std::cout << "Version: " << glGetString(GL_VERSION) << "\n";

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