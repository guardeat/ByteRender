#pragma once

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "core/window.h"
#include "core/mesh.h"
#include "render_array.h"

namespace Byte {

	struct OpenGLAPI {
        static void initialize(Window& window) {
            static bool initialized{ false };

            glfwMakeContextCurrent(&window.handle());
            glfwSetFramebufferSizeCallback(&window.handle(), OpenGLAPI::framebufferSizeCallback);

            if (!initialized) {
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

                if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                    throw std::exception{ "GLAD cannot be loaded" };
                }

                glEnable(GL_DEPTH_TEST);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                glPatchParameteri(GL_PATCH_VERTICES, 4);

                initialized = true;
            }
        }

        static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
            glViewport(0, 0, width, height);
        }

        static void update(Window& window) {
            glfwSwapBuffers(&window.handle());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        static void drawElements(size_t size, GLenum type = GL_TRIANGLES) {
            glDrawElements(type, static_cast<GLint>(size), GL_UNSIGNED_INT, 0);
        }

        static void drawInstancedElements(
            size_t size,
            size_t instanceCount,
            GLenum type = GL_TRIANGLES) {
            if (instanceCount) {
                GLint glSize{ static_cast<GLint>(size) };
                GLsizei glCount{ static_cast<GLsizei>(instanceCount) };
                glDrawElementsInstanced(type, glSize, GL_UNSIGNED_INT, 0, glCount);
            }
        }

        static void drawQuad() {
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        template<typename Type>
        static RenderBuffer buildRenderBuffer(
            const Vector<Type>& data,
            Layout layout, 
            BufferMode mode, 
            GLenum type,
            GLuint attributeStart = 0) {
            GLuint bufferID{ 0 };
            glGenBuffers(1, &bufferID);

            GLint target{ (type == GL_UNSIGNED_INT) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER };

            glBindBuffer(target, bufferID);
            glBufferData(target, data.size() * sizeof(Type), data.data(), convert(mode));

            size_t stride{ layout.stride() * sizeof(Type) };

            size_t offset{};
            for (GLuint attribIndex{}; attribIndex < layout.size(); ++attribIndex) {
                glEnableVertexAttribArray(attribIndex + attributeStart);
                glVertexAttribPointer(
                    attribIndex + attributeStart,
                    layout[attribIndex],
                    type,
                    GL_FALSE,
                    static_cast<GLsizei>(stride),
                    reinterpret_cast<const void*>(offset)
                );

                offset += layout[attribIndex] * sizeof(Type);
            }

            RenderBuffer buffer{};
            buffer.id = bufferID;
            buffer.layout = std::move(layout);
            buffer.mode = mode;

            return buffer;
        }

        static RenderArray buildRenderArray(Mesh& mesh) {
            RenderArray renderArray{};
            GLuint id;
            glGenVertexArrays(1, &id);
            glBindVertexArray(id);

            renderArray.id = static_cast<RenderArrayID>(id);

            RenderBuffer vertexBuffer{ buildRenderBuffer<float>(
                mesh.vertices(),
                mesh.layout(),
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_FLOAT
            ) };

            renderArray.renderBuffers.push_back(vertexBuffer);

            RenderBuffer indexBuffer{ buildRenderBuffer<uint32_t>(
                mesh.indices(),
                Layout{},
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_UNSIGNED_INT
            ) };

            renderArray.indexBuffer = indexBuffer;

            glBindVertexArray(0);

            return renderArray;
        }

        static GLenum convert(BufferMode mode) {
            switch (mode) {
                case BufferMode::STATIC:  
                    return GL_STATIC_DRAW;
                case BufferMode::DYNAMIC: 
                    return GL_DYNAMIC_DRAW;
                default: 
                    throw std::invalid_argument("Invalid BufferMode");
            }
        }
	};

}