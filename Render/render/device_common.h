#pragma once

#include <type_traits>
#include <stdexcept>

#include "core/core_types.h"
#include "core/asset.h"
#include "core/repository.h"
#include "render_types.h"
#include "framebuffer.h"
#include "texture.h"
#include "instance_group.h"

namespace Byte {

	template<typename RenderAPI, template<typename> class GPUResourceType>
	class GPUMemoryDevice {
	private:
		using GPUVariant =
			Variant<
			GPUResourceType<Mesh>,
			GPUResourceType<InstanceGroup>,
			GPUResourceType<Texture>>;

		Map<AssetID, GPUVariant> _data;

	public:
		GPUMemoryDevice() = default;

		GPUMemoryDevice(const GPUMemoryDevice& left) = delete;

		GPUMemoryDevice(GPUMemoryDevice&& right) noexcept = default;

		GPUMemoryDevice& operator=(const GPUMemoryDevice& left) = delete;

		GPUMemoryDevice& operator=(GPUMemoryDevice&& right) noexcept = default;

		~GPUMemoryDevice() {
			clear();
		}

		template<typename AssetType>
		GPUResourceType<AssetType>& get(const AssetType& asset) {
			auto it{ _data.find(asset.assetID()) };
			if (it != _data.end()) {
				return std::get<GPUResourceType<AssetType>>(it->second);
			}
			throw std::runtime_error("Asset not found in gpu memory");
		}

		template<typename AssetType>
		const GPUResourceType<AssetType>& get(const AssetType& asset) const {
			auto it{ _data.find(asset.assetID()) };
			if (it != _data.end()) {
				return std::get<GPUResourceType<AssetType>>(it->second);
			}
			throw std::runtime_error("Asset not found in gpu memory");
		}

		template<typename AssetType>
		void load(AssetType& asset) {
			auto gID{ RenderAPI::build(asset) };

			_data.emplace(asset.assetID(), std::move(gID));
		}

		void load(InstanceGroup& group, Mesh& mesh) {
			auto gID{ RenderAPI::build(group, mesh) };

			group.sync();
			gID.capacity = group.count();

			_data.emplace(group.assetID(), std::move(gID));
		}

		template<typename AssetType>
		void release(AssetType& asset) {
			auto it{ _data.find(asset.assetID()) };
			if (it != _data.end()) {
				RenderAPI::release(std::get<GPUResourceType<AssetType>>(it->second));
				_data.erase(it);
			}
		}

		template<typename AssetType>
		bool loaded(const AssetType& asset) const {
			return _data.contains(asset.assetID());
		}

		template<typename AssetType>
		void bind(const AssetType& asset) const {
			const GPUResourceType<AssetType>& resourceID{ get(asset) };

			if constexpr (std::is_same_v<AssetType, Texture>) {
				bind(asset, TextureUnit::UNIT_0);
			}

			else {
				RenderAPI::bind(resourceID);
			}
		}

		void bind(const Texture& texture, TextureUnit unit) const {
			GPUResourceType<Texture> resourceID{ get(texture) };
			RenderAPI::bind(resourceID, unit);
		}

		void bind(size_t width, size_t height) const {
			RenderAPI::bind(width, height);
		}

		void update(InstanceGroup& group, float capacityMultiplier = 2.0f) {
			GPUResourceType<InstanceGroup>& bufferGroup{ get(group) };
			size_t size{ group.data().size() };
			if (size > bufferGroup.capacity) {
				size_t newSize{ static_cast<size_t>(group.data().size() * capacityMultiplier) };
				bufferGroup.capacity = newSize;
				OpenGL::bufferData(bufferGroup.renderBuffers[1], group.data(), newSize, false);
			}
			else {
				OpenGL::subBufferData(bufferGroup.renderBuffers[1], group.data());
			}

			group.sync();
		}

		Map<AssetID, GPUVariant>& data() {
			return _data;
		}

		const Map<AssetID, GPUVariant>& data() const {
			return _data;
		}

		void clear() {
			for (auto& [assetID, resource] : _data) {
				std::visit([](auto& res) {
					RenderAPI::release(res);
					}, resource);
			}
			_data.clear();
		}
	};

	template<
		typename RenderAPI, 
		template<typename> class GPUResourceType,
		typename GPUMemoryDeviceType>
	class GPUShaderDevice {
	private:
		GPUMemoryDeviceType* _memory{ nullptr };

		Map<AssetID, GPUResourceType<Shader>> _shaders;

	public:
		GPUShaderDevice(GPUMemoryDeviceType& device)
			:_memory{ &device } {
		}

		~GPUShaderDevice() {
			clear();
		}

		GPUShaderDevice(const GPUShaderDevice&) = delete;

		GPUShaderDevice(GPUShaderDevice&& other) noexcept {
			clear();
			_shaders = std::move(other._shaders);
		}

		GPUShaderDevice& operator=(const GPUShaderDevice&) = delete;

		GPUShaderDevice& operator=(GPUShaderDevice&& other) noexcept {
			if (this != &other) {
				clear();
				_shaders = std::move(other._shaders);
			}
			return *this;
		}

		void memory(GPUMemoryDeviceType& device) {
			_memory = &device;
		}

		template<typename Type>
		void set(const Shader& shader, const Tag& tag,const Type& value) const {
			GPUResourceType<Shader> id{ _shaders.at(shader.assetID()) };
			RenderAPI::uniform(id, tag, value);
		}

		template<>
		void set(const Shader& shader, const Tag& tag, const TextureUnit& unit) const {
			GPUResourceType<Shader> id{ _shaders.at(shader.assetID()) };
			RenderAPI::uniform(id, tag, static_cast<int>(unit));
		}

		void set(const Shader& shader, const Material& material, const Repository& repository) const {
			GPUResourceType<Shader> id{ _shaders.at(shader.assetID()) };

			if (shader.useDefaultMaterial()) {
				int32_t materialMode{};
				constexpr size_t ALBEDO_BIT{ 0 };
				constexpr size_t MATERIAL_BIT{ 1 };

				if( material.albedoTexture() != 0) {
					materialMode |= (1 << ALBEDO_BIT);
					const Texture& texture{ repository.texture(material.albedoTexture()) };
					_memory->bind(texture, TextureUnit::UNIT_0);
					set(shader, "uAlbedoTexture", TextureUnit::UNIT_0);
				}
				else {
					RenderAPI::uniform(id, "uAlbedo", material.color());
				}

				if( material.materialTexture() != 0) {
					materialMode |= (1 << MATERIAL_BIT);
					const Texture& texture{ repository.texture(material.materialTexture()) };
					TextureUnit textureUnit{ static_cast<TextureUnit>(materialMode) };
					_memory->bind(texture, textureUnit);
					set(shader, "uMaterialTexture", textureUnit);
				}
				else {
					RenderAPI::uniform(id, "uMetallic", material.metallic());
					RenderAPI::uniform(id, "uRoughness", material.roughness());

					RenderAPI::uniform(id, "uEmission", material.emission());
					RenderAPI::uniform(id, "uAO", material.ambientOcclusion());
				}

				RenderAPI::uniform(id, "uMaterialMode", materialMode);
			}

			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &id, &shader](const auto& inputValue) {
						OpenGL::uniform(id, tag, inputValue);
						}, input);
				}
			}
		}

		void set(const Shader& shader, const Transform& transform) const {
			GPUResourceType<Shader> id{ _shaders.at(shader.assetID()) };
			RenderAPI::uniform(id, "uPosition", transform.position());
			RenderAPI::uniform(id, "uScale", transform.scale());
			RenderAPI::uniform(id, "uRotation", transform.rotation());
		}

		void bind(const Shader& shader) const {
			GPUResourceType<Shader> resourceID{ _shaders.at(shader.assetID()) };
			RenderAPI::bind(resourceID);
		}

		void build(Shader& shader) {
			GPUResourceType<Shader> shaderID{ RenderAPI::build(shader) };
			_shaders.emplace(shader.assetID(), std::move(shaderID));
		}

		bool built(const Shader& shader) const {
			return _shaders.contains(shader.assetID());
		}

		void release(const Shader& shader) {
			auto it{ _shaders.find(_shaders.at(shader.assetID())) };
			if (it != _shaders.end()) {
				RenderAPI::release(it->second);
				_shaders.erase(it);
			}
		}

		void clear() {
			for (auto& [assetID, resource] : _shaders) {
				RenderAPI::release(resource);
			}
			_shaders.clear();
		}
	};

	template<
		typename RenderAPI, 
		template<typename> class GPUResourceType,
		typename GPUMemoryDeviceType>
	class GPUFramebufferDevice {
	private:
		GPUMemoryDeviceType* _memory{ nullptr };

		Map<AssetID, GPUResourceType<Framebuffer>> _framebuffers;

	public:
		GPUFramebufferDevice(GPUMemoryDeviceType& device)
			:_memory{ &device } {
		}

		GPUFramebufferDevice(const GPUFramebufferDevice&) = delete;

		GPUFramebufferDevice(GPUFramebufferDevice&& other) noexcept {
			clear();
			_memory = other._memory;
			_framebuffers = std::move(other._framebuffers);
			other._memory = nullptr;
		}

		GPUFramebufferDevice& operator=(const GPUFramebufferDevice&) = delete;

		GPUFramebufferDevice& operator=(GPUFramebufferDevice&& other) noexcept {
			if (this != &other) {
				clear();
				_memory = other._memory;
				_framebuffers = std::move(other._framebuffers);
				other._memory = nullptr;
			}

			return *this;
		}

		~GPUFramebufferDevice() {
			clear();
		}

		void memory(GPUMemoryDeviceType& device) {
			_memory = &device;
		}

		void viewport(size_t width, size_t height) {
			RenderAPI::viewport(width, height);
		}

		void clearBuffer() {
			RenderAPI::clear();
		}

		void resize(Framebuffer& buffer, size_t width, size_t height) {
			if (buffer.resize()) {
				AssetID assetID{ buffer.assetID() };

				Vector<GPUResourceID> ids;
				for (auto& [_, texture] : buffer.textures()) {
					ids.push_back(_memory->get(texture).id);

					texture.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
					texture.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
					_memory->data().erase(texture.assetID());
				}

				GPUResourceType<Framebuffer>& id{ _framebuffers.at(assetID) };
				RenderAPI::release(id, ids);
				_framebuffers.erase(assetID);

				buffer.attachments().clear();
				buffer.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
				buffer.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));

				build(buffer);
			}
		}

		void draw(size_t size, DrawType drawType = DrawType::TRIANGLES) {
			RenderAPI::draw(size, drawType);
		}

		void draw(size_t size, size_t instanceCount, DrawType drawType = DrawType::TRIANGLES) {
			RenderAPI::draw(size, instanceCount, drawType);
		}

		void bind(const Framebuffer& buffer) const {
			GPUResourceType<Framebuffer> resourceID{ _framebuffers.at(buffer.assetID()) };
			RenderAPI::bind(buffer, resourceID);
		}

		void build(Framebuffer& buffer) {
			auto [id, textures] = RenderAPI::build(buffer);

			for (auto [assetID, gID] : textures) {
				_memory->data().emplace(assetID, gID);
				id.textures.push_back(assetID);
			}

			_framebuffers.emplace(buffer.assetID(), std::move(id));
		}

		bool built(const Framebuffer& buffer) const {
			return _framebuffers.contains(buffer.assetID());
		}

		void release(const Framebuffer& buffer) {
			Vector<GPUResourceID> ids;
			for (auto& [_, texture] : buffer.textures()) {
				const GPUResourceType<Texture>& textureID{ _memory->get(texture) };
				ids.push_back(textureID.id);
				_memory->data().erase(texture.assetID());
			}

			GPUResourceType<Framebuffer>& bufferID{ _framebuffers.at(buffer.assetID()) };

			RenderAPI::release(bufferID, ids);
			_framebuffers.erase(buffer.assetID());
		}

		void clear() {
			Vector<GPUResourceType<Texture>> textures;

			for (auto& [assetID, buffer] : _framebuffers) {
				Vector<GPUResourceID> textures;

				for (auto assetID : buffer.textures) {
					if (_memory->data().contains(assetID)) {
						auto& gpuID{ std::get<GPUResourceType<Texture>>(_memory->data().at(assetID)) };
						textures.push_back(gpuID.id);
						_memory->data().erase(assetID);
					}
				}
				
				RenderAPI::release(buffer, textures);
			}
		}

	};

}
