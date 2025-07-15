#pragma once

#include "core/math/quaternion.h"
#include "core/transform.h"
#include "render/renderer.h"

namespace Byte {

	class CameraController {
	private:
		float yaw{};
		float pitch{};
		float oldX{};
		float oldY{};

		float speed{ 50.0f };
		float sensitivity{ 0.1f };

	public:
		CameraController() = default;

		CameraController(float speed, float sensitivity)
			:speed{ speed }, sensitivity{ sensitivity } {
		}

		Quaternion calculateRotation(float newX, float newY) {
			float offsetX{ newX - oldX };
			float offsetY{ newY - oldY };
			oldX = newX;
			oldY = newY;

			offsetX *= sensitivity;
			offsetY *= sensitivity;

			yaw -= offsetX;
			pitch -= offsetY;

			if (pitch > 89.0f)
			{
				pitch = 89.0f;
			}
			if (pitch < -89.0f)
			{
				pitch = -89.0f;
			}

			Quaternion pitchQuaternion(Vec3{ 1, 0, 0 }, pitch);
			Quaternion yawQuaternion(Vec3{ 0, 1, 0 }, yaw);

			return yawQuaternion * pitchQuaternion;
		}

		void update(Window& window, Transform& transform, float dt) {

			double xpos, ypos;
			glfwGetCursorPos(&window.handle(), &xpos, &ypos);

			transform.rotation(calculateRotation(static_cast<float>(xpos), static_cast<float>(ypos)));

			Vec3 offset{};
			if (glfwGetKey(&window.handle(), GLFW_KEY_W) == GLFW_PRESS) {
				offset += transform.front();
			}
			if (glfwGetKey(&window.handle(), GLFW_KEY_S) == GLFW_PRESS) {
				offset -= transform.front();
			}
			if (glfwGetKey(&window.handle(), GLFW_KEY_A) == GLFW_PRESS) {
				offset -= transform.right();
			}
			if (glfwGetKey(&window.handle(), GLFW_KEY_D) == GLFW_PRESS) {
				offset += transform.right();
			}

			if (offset.length() > 0) {
				transform.position(transform.position() + offset.normalized() * speed * dt);
			}

			glfwSetInputMode(&window.handle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	};

}