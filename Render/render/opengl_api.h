#pragma once

#include <fstream>

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "core/window.h"
#include "core/mesh.h"
#include "core/byte_math.h"
#include "render_types.h"
#include "framebuffer.h"
#include "texture.h"
#include "instance_group.h"

namespace Byte {

    struct OpenGL {
        static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
            glViewport(0, 0, width, height);
        }

        static void initialize(Window& window) {
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

        static void update(Window& window) {
            glfwSwapBuffers(&window.handle());
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        static void clear() {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        static void state(RenderState state) {
            switch (state) {
            case RenderState::ENABLE_DEPTH:
                glEnable(GL_DEPTH_TEST);
                break;
            case RenderState::DISABLE_DEPTH:
                glDisable(GL_DEPTH_TEST);
                break;
            case RenderState::ENABLE_BLEND:
                glEnable(GL_BLEND);
                break;
            case RenderState::DISABLE_BLEND:
                glDisable(GL_BLEND);
                break;
            case RenderState::ENABLE_CULLING:
                glEnable(GL_CULL_FACE);
                break;
            case RenderState::DISABLE_CULLING:
                glDisable(GL_CULL_FACE);
                break;
            case RenderState::CULL_BACK:
                glCullFace(GL_BACK);
                break;
            case RenderState::CULL_FRONT:
                glCullFace(GL_FRONT);
                break;
			case RenderState::BLEND_ADD:
                glBlendFunc(GL_ONE, GL_ONE);
                break;
            }
        }

        static GLenum convert(AttachmentType type) {
            switch (type) {
            case AttachmentType::COLOR_0:
                return GL_COLOR_ATTACHMENT0;
            case AttachmentType::COLOR_1:
                return GL_COLOR_ATTACHMENT1;
            case AttachmentType::COLOR_2:
                return GL_COLOR_ATTACHMENT2;
            case AttachmentType::COLOR_3:
                return GL_COLOR_ATTACHMENT3;
            case AttachmentType::DEPTH:
                return GL_DEPTH_ATTACHMENT;
            default:
                throw std::invalid_argument("Invalid AttachmentType");
            }
        }

        static GLenum convert(DataType type) {
            switch (type) {
            case DataType::BYTE:
                return GL_BYTE;
            case DataType::UNSIGNED_BYTE:
                return GL_UNSIGNED_BYTE;
            case DataType::SHORT:
                return GL_SHORT;
            case DataType::UNSIGNED_SHORT:
                return GL_UNSIGNED_SHORT;
            case DataType::INT:
                return GL_INT;
            case DataType::UNSIGNED_INT:
                return GL_UNSIGNED_INT;
            case DataType::FLOAT:
                return GL_FLOAT;
            default:
                throw std::invalid_argument("Invalid DataType");
            }
        }

        static GLenum convert(ColorFormat format) {
            switch (format) {
            case ColorFormat::DEPTH:
                return GL_DEPTH_COMPONENT;
            case ColorFormat::RED:
                return GL_RED;
            case ColorFormat::GREEN:
                return GL_GREEN;
            case ColorFormat::BLUE:
                return GL_BLUE;
            case ColorFormat::ALPHA:
                return GL_ALPHA;
            case ColorFormat::RGB:
                return GL_RGB;
            case ColorFormat::RGBA:
                return GL_RGBA;
            case ColorFormat::RGBA32F:
                return GL_RGBA32F;
            case ColorFormat::RGB32F:
                return GL_RGB32F;
            case ColorFormat::RGBA16F:
                return GL_RGBA16F;
            case ColorFormat::RGB16F:
                return GL_RGB16F;
            case ColorFormat::R11F_G11F_B10F:
                return GL_R11F_G11F_B10F;
            case ColorFormat::R16F:
                return GL_R16F;
            case ColorFormat::R32F:
                return GL_R32F;
            case ColorFormat::R16:
                return GL_R16;
            case ColorFormat::RGB16:
                return GL_RGB16;
            case ColorFormat::RGBA16:
                return GL_RGBA16;
            default:
                throw std::invalid_argument("Invalid ColorFormat");
            }
        }

        static GLenum convert(TextureUnit unit) {
            switch (unit) {
            case TextureUnit::UNIT_0:
                return GL_TEXTURE0;
            case TextureUnit::UNIT_1:
                return GL_TEXTURE1;
            case TextureUnit::UNIT_2:
                return GL_TEXTURE2;
            case TextureUnit::UNIT_3:
                return GL_TEXTURE3;
            case TextureUnit::UNIT_4:
                return GL_TEXTURE4;
            case TextureUnit::UNIT_5:
                return GL_TEXTURE5;
            case TextureUnit::UNIT_6:
                return GL_TEXTURE6;
            case TextureUnit::UNIT_7:
                return GL_TEXTURE7;
            default:
                return GL_TEXTURE0;
            }
        }

        static GLenum convert(TextureFilter filter) {
            switch (filter) {
            case TextureFilter::NEAREST:
                return GL_NEAREST;
            case TextureFilter::LINEAR:
                return GL_LINEAR;
            case TextureFilter::NEAREST_MIPMAP_NEAREST:
                return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LINEAR_MIPMAP_NEAREST:
                return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NEAREST_MIPMAP_LINEAR:
                return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LINEAR_MIPMAP_LINEAR:
                return GL_LINEAR_MIPMAP_LINEAR;
            default:
                throw std::invalid_argument("Invalid TextureFilter");
            }
        }

        static GLenum convert(TextureWrap wrap) {
            switch (wrap) {
            case TextureWrap::REPEAT:
                return GL_REPEAT;
            case TextureWrap::MIRRORED_REPEAT:
                return GL_MIRRORED_REPEAT;
            case TextureWrap::CLAMP_TO_EDGE:
                return GL_CLAMP_TO_EDGE;
            case TextureWrap::CLAMP_TO_BORDER:
                return GL_CLAMP_TO_BORDER;
            default:
                throw std::invalid_argument("Invalid TextureWrap");
            }
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

        static void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
            glDrawElements(convert(drawType), static_cast<GLint>(size), GL_UNSIGNED_INT, 0);
        }

        static void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
            if (instanceCount) {
                GLint glSize{ static_cast<GLint>(size) };
                GLsizei glCount{ static_cast<GLsizei>(instanceCount) };
                glDrawElementsInstanced(convert(drawType), glSize, GL_UNSIGNED_INT, 0, glCount);
            }
        }

        static void bind(GPUBufferGroup id = 0) {
            glBindVertexArray(id);
        }

        template<typename Type>
        static GPUBuffer build(
            const Vector<Type>& data,
            Layout layout,
            BufferMode mode,
            GLenum type,
            GLuint attributeStart = 0,
            bool instanced = false
        ) {
            GLuint bufferID{};
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
                if (instanced) {
                    glVertexAttribDivisor(attribIndex + attributeStart, 1);
                }
                offset += layout[attribIndex] * sizeof(Type);
            }

            return bufferID;
        }

        static GPUBufferGroup build(Mesh& mesh) {
            GPUBufferGroup bufferGroup{};
            GLuint id;
            glGenVertexArrays(1, &id);
            glBindVertexArray(id);

            bufferGroup.id = static_cast<GPUBuffer>(id);

            GPUBuffer vertexBuffer{ build<float>(
                mesh.vertices(),
                mesh.layout(),
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_FLOAT
            ) };

            bufferGroup.renderBuffers.push_back(vertexBuffer);

            GPUBuffer indexBuffer{ build<uint32_t>(
                mesh.indices(),
                Layout{},
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_UNSIGNED_INT
            ) };

            bufferGroup.indexBuffer = indexBuffer;

            glBindVertexArray(0);

            return  bufferGroup;
        }

        static GPUBufferGroup build(InstanceGroup& group, Mesh& mesh) {
            GPUBufferGroup bufferGroup{};

            GLuint vao{};
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            bufferGroup.id = static_cast<GPUBuffer>(vao);

            GPUBuffer vertexBuffer{ build<float>(
                mesh.vertices(),
                mesh.layout(),
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_FLOAT
            )};

            bufferGroup.renderBuffers.push_back(vertexBuffer);

            GPUBuffer indexBuffer{ build<uint32_t>(
                mesh.indices(),
                Layout{},
                mesh.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                GL_UNSIGNED_INT
            )};

            bufferGroup.indexBuffer = indexBuffer;

            GLuint attribIndex{ static_cast<GLuint>(mesh.layout().size()) };
            GPUBuffer instanceBuffer{ build<float>(
                    group.data(),
                    group.layout(),
                    group.dynamic() ? BufferMode::DYNAMIC : BufferMode::STATIC,
                    GL_FLOAT,
                    attribIndex,
                    true
            )};

            bufferGroup.renderBuffers.push_back(instanceBuffer);

            glBindVertexArray(0);

            return bufferGroup;
        }

        static void release(GPUBufferGroup bufferGroup) {
            if (bufferGroup.indexBuffer != 0) {
                glDeleteBuffers(1, &bufferGroup.indexBuffer.id);
            }

            for (auto& buffer : bufferGroup.renderBuffers) {
                if (buffer != 0) {
                    glDeleteBuffers(1, &buffer.id);
                }
            }

            if (bufferGroup.id != 0) {
                glDeleteVertexArrays(1, &bufferGroup.id);
            }
        }

        template<typename T>
        static void bufferData(GPUBuffer buffer, const Vector<T>& data, size_t size, bool dynamic = false) {
            glBindBuffer(GL_ARRAY_BUFFER, buffer.id);
            glBufferData(GL_ARRAY_BUFFER, size * sizeof(T), data.data(), dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        }

        template<typename T>
        static void subBufferData(GPUBuffer buffer, const Vector<T>& data, size_t offset = 0) {
            glBindBuffer(GL_ARRAY_BUFFER, buffer.id);
            glBufferSubData(GL_ARRAY_BUFFER, offset, data.size() * sizeof(T), data.data());
        }

        static void bind(GPUShader id = 0) {
            glUseProgram(id);
        }

        template<typename Type>
        static void uniform(GPUShader id, const std::string& name, const Type& value) {
            throw std::exception("No such uniform type");
        }

        template<>
        static void uniform<bool>(GPUShader id, const std::string& name, const bool& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform1i(loc, (int)value);
        }

        template<>
        static void uniform<int>(GPUShader id, const std::string& name, const int& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform1i(loc, value);
        }

        template<>
        static void uniform<size_t>(GPUShader id, const std::string& name, const size_t& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform1i(loc, static_cast<GLint>(value));
        }

        template<>
        static void uniform<float>(GPUShader id, const std::string& name, const float& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform1f(loc, value);
        }

        template<>
        static void uniform<Vec2>(GPUShader id, const std::string& name, const Vec2& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform2f(loc, value.x, value.y);
        }

        template<>
        static void uniform<Vec3>(GPUShader id, const std::string& name, const Vec3& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform3f(loc, value.x, value.y, value.z);
        }

        template<>
        static void uniform<Vec4>(GPUShader id, const std::string& name, const Vec4& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform4f(loc, value.x, value.y, value.z, value.w);
        }

        template<>
        static void uniform<Quaternion>(GPUShader id, const std::string& name, const Quaternion& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniform4f(loc, value.x, value.y, value.z, value.w);
        }

        template<>
        static void uniform<Mat2>(GPUShader id, const std::string& name, const Mat2& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniformMatrix2fv(loc, 1, GL_FALSE, value.data);
        }

        template<>
        static void uniform<Mat3>(GPUShader id, const std::string& name, const Mat3& value) {
            auto loc{ glGetUniformLocation(id, name.c_str()) };
            glUniformMatrix3fv(loc, 1, GL_FALSE, value.data);
        }

        template<>
        static void uniform<Mat4>(GPUShader id, const std::string& name, const Mat4& value) {
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

        static void release(GPUShader id) {
            glDeleteProgram(id);
        }

        static GPUShader build(
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

            glDeleteShader(vertex);
            glDeleteShader(fragment);
            if (geometry) {
                glDeleteShader(geometry);
            }

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

        static void bind(GPUTexture id, TextureUnit unit = TextureUnit::UNIT_0) {
            glActiveTexture(convert(unit));
            glBindTexture(GL_TEXTURE_2D, id);
        }

        static void release(GPUTexture id) {
            glDeleteTextures(1, &id.id);
        }

        static GPUTexture build(Texture& texture) {
            GPUTexture textureID;

            GLint glWidth{ static_cast<GLint>(texture.width()) };
            GLint glHeight{ static_cast<GLint>(texture.height()) };

            glGenTextures(1, &textureID.id);

            glBindTexture(GL_TEXTURE_2D, textureID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, convert(texture.wrapS()));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, convert(texture.wrapT()));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, convert(texture.minFilter()));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, convert(texture.magFilter()));

            uint8_t* textureData{ texture.data().empty() ? nullptr : texture.data().data() };

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(
                GL_TEXTURE_2D, 0,
                convert(texture.internalFormat()),
                glWidth, glHeight, 0,
                convert(texture.format()),
                convert(texture.dataType()),
                textureData);

            glGenerateMipmap(GL_TEXTURE_2D);

            glBindTexture(GL_TEXTURE_2D, 0);

            return textureID;
        }

        static void bind(const Framebuffer& buffer, GPUFramebuffer id) {
            glBindFramebuffer(GL_FRAMEBUFFER, id);

            if (!buffer.attachments().empty()) {
                Vector<uint32_t> attachments;
                for (auto& att : buffer.attachments()) {
                    attachments.push_back(convert(att));
                }
                glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
            }

            glViewport(0, 0, static_cast<GLsizei>(buffer.width()), static_cast<GLsizei>(buffer.height()));
        }

        static void bind(size_t width, size_t height) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        }

        static Pair<GPUFramebuffer, Map<AssetID, GPUTexture>> build(Framebuffer& buffer) {
            GPUFramebuffer id{};
            Map<AssetID, GPUTexture> textureIDs{};
            glGenFramebuffers(1, &id.id);
            glBindFramebuffer(GL_FRAMEBUFFER, id);

            GLint glWidth{ static_cast<GLint>(buffer.width()) };
            GLint glHeight{ static_cast<GLint>(buffer.height()) };

            bool hasDepth{ false };

            for (auto& [tag, att] : buffer.textures()) {
                size_t width{ att.width() ? att.width() : buffer.width() };
                size_t height{ att.height() ? att.height() : buffer.height() };

                att.width(width);
                att.height(height);

                GPUTexture attID{ build(att) };
                textureIDs.emplace(att.assetID(), attID);
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER, convert(att.attachment()), GL_TEXTURE_2D, attID, 0);

                if (att.attachment() == AttachmentType::DEPTH) {
                    hasDepth = true;
                }
                else {
                    buffer.attachments().push_back(att.attachment());
                }
            }

            std::sort(buffer.attachments().begin(), buffer.attachments().end(),
                [](AttachmentType a, AttachmentType b) {
                    return static_cast<uint8_t>(a) < static_cast<uint8_t>(b);
                });

            if (!hasDepth) {
                unsigned int rboDepth;
                glGenRenderbuffers(1, &rboDepth);
                glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, glWidth, glHeight);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
            }

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                Vector<GPUResourceID> toDelete{};
                for (auto [_, deleteID] : textureIDs) {
                    toDelete.push_back(deleteID.id);
                }
                release(id, toDelete);
                throw std::exception("Framebuffer not complete");
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            return std::make_pair(id, std::move(textureIDs));
        }

        static void release(GPUFramebuffer id, const Vector<GPUResourceID>& textureIDs) {
            if (!textureIDs.empty()) {
                glDeleteTextures(static_cast<GLsizei>(textureIDs.size()), textureIDs.data());
            }

            if (id) {
                glDeleteFramebuffers(1, &id.id);
            }
        }

    };
}