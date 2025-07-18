#pragma once

#include <fstream>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "core/window.h"
#include "core/mesh.h"
#include "core/byte_math.h"
#include "render_array.h"

namespace Byte::OpenGL {

        inline void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
            glViewport(0, 0, width, height);
        }

        inline void initialize(Window& window) {
            static bool initialized{ false };

            glfwMakeContextCurrent(&window.handle());
            glfwSetFramebufferSizeCallback(&window.handle(), framebufferSizeCallback);

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

        inline void update(Window& window) {
            glfwSwapBuffers(&window.handle());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        struct Draw {
            static void elements(size_t size, DrawType drawType = DrawType::TRIANGLES) {
                glDrawElements(convert(drawType), static_cast<GLint>(size), GL_UNSIGNED_INT, 0);
            }

            static void elementsInstanced(
                size_t size,
                size_t instanceCount,
                DrawType drawType = DrawType::TRIANGLES) {
                if (instanceCount) {
                    GLint glSize{ static_cast<GLint>(size) };
                    GLsizei glCount{ static_cast<GLsizei>(instanceCount) };
                    glDrawElementsInstanced(convert(drawType), glSize, GL_UNSIGNED_INT, 0, glCount);
                }
            }

            static GLenum convert(DrawType type) {
                switch (type) {
                case DrawType::POINTS:        
                    return GL_POINTS;
                case DrawType::LINES:         
                    return GL_LINES;
                case DrawType::LINE_LOOP:      
                    return GL_LINE_LOOP;
                case DrawType::LINE_STRIP:    
                    return GL_LINE_STRIP;
                case DrawType::TRIANGLES:      
                    return GL_TRIANGLES;
                case DrawType::TRIANGLE_STRIP: 
                    return GL_TRIANGLE_STRIP;
                case DrawType::TRIANGLE_FAN:   
                    return GL_TRIANGLE_FAN;
                default:
                    throw std::invalid_argument("Invalid DrawType");
                }
            }
        };

        struct Memory {
            static void bind(uint32_t id = 0) {
                glBindVertexArray(id);
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

        struct Shader {
            static void bind(uint32_t id = 0) {
                glUseProgram(id);
            }

            static void releaseShader(uint32_t id) {
                glDeleteShader(id);
            }

            template<typename Type>
            static void uniform(uint32_t id, const std::string& name, const Type& value) {
                throw std::exception("No such uniform type");
            }

            template<>
            static void uniform<bool>(uint32_t id, const std::string& name, const bool& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform1i(loc, (int)value);
            }

            template<>
            static void uniform<int>(uint32_t id, const std::string& name, const int& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform1i(loc, value);
            }

            template<>
            static void uniform<size_t>(uint32_t id, const std::string& name, const size_t& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform1i(loc, static_cast<GLint>(value));
            }

            template<>
            static void uniform<float>(uint32_t id, const std::string& name, const float& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform1f(loc, value);
            }

            template<>
            static void uniform<Vec2>(uint32_t id, const std::string& name, const Vec2& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform2f(loc, value.x, value.y);
            }

            template<>
            static void uniform<Vec3>(uint32_t id, const std::string& name, const Vec3& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform3f(loc, value.x, value.y, value.z);
            }

            template<>
            static void uniform<Vec4>(uint32_t id, const std::string& name, const Vec4& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform4f(loc, value.x, value.y, value.z, value.w);
            }

            template<>
            static void uniform<Quaternion>(uint32_t id, const std::string& name, const Quaternion& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniform4f(loc, value.x, value.y, value.z, value.w);
            }

            template<>
            static void uniform<Mat2>(uint32_t id, const std::string& name, const Mat2& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniformMatrix2fv(loc, 1, GL_FALSE, value.data);
            }

            template<>
            static void uniform<Mat3>(uint32_t id, const std::string& name, const Mat3& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniformMatrix3fv(loc, 1, GL_FALSE, value.data);
            }

            template<>
            static void uniform<Mat4>(uint32_t id, const std::string& name, const Mat4& value) {
                auto loc{ glGetUniformLocation(id, name.c_str()) };
                glUniformMatrix4fv(loc, 1, GL_FALSE, value.data);
            }

            static uint32_t compileShader(const Path& shaderPath, ShaderType shaderType) {
                std::string shaderCode;
                std::ifstream shaderFile;

                shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

                shaderFile.open(shaderPath);
                std::stringstream shaderStream;

                shaderStream << shaderFile.rdbuf();

                shaderFile.close();

                shaderCode = shaderStream.str();

                const char* sCode{ shaderCode.c_str() };

                uint32_t id{ glCreateShader(convert(shaderType)) };
                glShaderSource(id, 1, &sCode, NULL);
                glCompileShader(id);
                checkShader(id);

                return id;
            }

            static void checkShader(uint32_t shader) {
                int success;
                char infoLog[1024];

                glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
                if (!success) {
                    glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                    throw std::exception{ infoLog };
                }
            }

            static void releaseProgram(uint32_t id) {
                glDeleteProgram(id);
            }

            static uint32_t buildProgram(
                uint32_t vertex,
                uint32_t fragment,
                uint32_t geometry = 0) {
                uint32_t id{ glCreateProgram() };

                glAttachShader(id, vertex);
                glAttachShader(id, fragment);

                if (geometry) {
                    glAttachShader(id, geometry);
                }

                glLinkProgram(id);
                checkProgram(id);

                return id;
            }

            static void checkProgram(uint32_t program) {
                int success;
                char infoLog[1024];

                glGetProgramiv(program, GL_LINK_STATUS, &success);
                if (!success) {
                    glGetShaderInfoLog(program, 1024, NULL, infoLog);
                    throw std::exception{ infoLog };
                }

            }

            static GLenum convert(ShaderType type) {
                switch (type) {
                case ShaderType::VERTEX:
                    return GL_VERTEX_SHADER;
                case ShaderType::FRAGMENT:
                    return GL_FRAGMENT_SHADER;
                case ShaderType::GEOMETRY:
                    return GL_GEOMETRY_SHADER;
                default:
                    throw std::invalid_argument("Invalid BufferMode");
                }
            }
        };
}