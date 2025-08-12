#pragma once

#include "render/render.h"
#include "scene/scene.h"
#include "core/window.h"
#include "core/timer.h"
#include "camera_controller.h"

namespace Byte {

	void buildTestScene(Scene& scene, Renderer& renderer);
	void debugLog(float dt);

	class Application {
	private:
		Renderer _renderer;
		Scene _scene;
		Window _window;
		CameraController _temp;

	public:
		void initialize(size_t width, size_t height, const std::string& title = "") {
			_window.initialize(width, height, title);

			_renderer = Renderer::build<SkyboxPass, ShadowPass, GeometryPass, LightingPass, BloomPass, DrawPass>();
			buildTestScene(_scene, _renderer);
			_renderer.parameter("point_light_group_id", _scene.pointLightGroup());
			_renderer.parameter("default_shader_path", Path{ "../Render/shader/" });
			_renderer.initialize(_window);
		}

		void run() {
			Timer timer;
			while (!_window.shouldClose()) {
				float dt{ timer.elapsed() };
				timer.reset();

				_window.pollEvents();
				RenderContext context{ _scene.renderContext() };
				_renderer.render(context);
				_renderer.update(_window);

				_scene.update(dt);
				_temp.update(_window, context.camera().second, dt);

				debugLog(dt);
			}
		}

	};

	void buildTestScene(Scene& scene, Renderer& renderer) {
		Mesh sphereMesh{ Primitive::sphere(20) };
		Material sphereMaterial{};
		sphereMaterial.color({ 0.9f, 0.1f, 0.2f, 1.0f });
		sphereMaterial.emission(0.5f);

		InstanceGroup sphereGroup{ sphereMesh.assetID(), sphereMaterial.assetID() };
		scene.repository().mesh(sphereMesh.assetID(), std::move(sphereMesh));
		scene.repository().material(sphereMaterial.assetID(), std::move(sphereMaterial));

		for (size_t i{ 0 }; i < 10; ++i) {
			for (size_t j{ 0 }; j < 10; ++j) {
				for (size_t k{ 0 }; k < 10; ++k) {
					Vec3 position{ static_cast<float>(i), static_cast<float>(j), static_cast<float>(k) };

					InstanceRenderer rendererComponent{ sphereGroup.assetID() };
					EntityID meshEntity{ scene.world().create<InstanceRenderer, Transform>(std::move(rendererComponent), Transform{})};
					auto& meshTransform{ scene.world().get<Transform>(meshEntity)};
					meshTransform.position(position);
					sphereGroup.submit(meshEntity, meshTransform);
				}
			}
		}

		scene.repository().instanceGroup(sphereGroup.assetID(), std::move(sphereGroup));

		Mesh groundMesh{ Primitive::cube() };
		Material groundMaterial{};
		groundMaterial.color({ 0.2f, 0.9f, 0.3f, 1.0f });

		MeshRenderer groundRenderer{ groundMesh.assetID(), groundMaterial.assetID() };
		EntityID groundEntity{ scene.world().create<MeshRenderer, Transform>(std::move(groundRenderer), Transform{})};
		scene.world().get<Transform>(groundEntity).position({0.0f, -1.0f, 0.0f});
		scene.world().get<Transform>(groundEntity).scale({200.0f, 1.0f, 200.05f});

		scene.repository().mesh(groundMesh.assetID(), std::move(groundMesh));
		scene.repository().material(groundMaterial.assetID(), std::move(groundMaterial));

		Material skybox{};
		skybox.parameter("uScatter", Vec3{ 0.1f, 0.2f, 0.9f });
		renderer.parameter("skybox_material", skybox.assetID());
		scene.repository().material(skybox.assetID(), std::move(skybox));

		scene.world().create<PointLight, Transform>(PointLight{}, Transform{});
	}

	void debugLog(float dt) {
		static bool firstRun{ true };
		static float fpsTimer{ 0.0f };
		static size_t frameCount{ 0 };

		if (firstRun) {
			std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";
			std::cout << "Vendor: " << glGetString(GL_VENDOR) << "\n";
			std::cout << "Version: " << glGetString(GL_VERSION) << "\n";

			firstRun = false;
		}

		fpsTimer += dt;
		frameCount++;

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

}