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
			auto gResource{ RenderAPI::build(asset) };

			_data.emplace(asset.assetID(), std::move(gResource));
		}

		void load(InstanceGroup& group, Mesh& mesh) {
			auto gResource{ RenderAPI::build(group, mesh) };

			group.sync();
			gResource.capacity = group.count();

			_data.emplace(group.assetID(), std::move(gResource));
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
			const GPUResourceType<AssetType>& gResource{ get(asset) };

			if constexpr (std::is_same_v<AssetType, Texture>) {
				bind(asset, TextureUnit::UNIT_0);
			}

			else {
				RenderAPI::bind(gResource);
			}
		}

		void bind(const Texture& texture, TextureUnit unit) const {
			const GPUResourceType<Texture>& gResource{ get(texture) };
			RenderAPI::bind(gResource, unit);
		}

		void bind(size_t width, size_t height) const {
			RenderAPI::bind(width, height);
		}

		void update(InstanceGroup& group, float capacityMultiplier = 2.0f) {
			GPUResourceType<InstanceGroup>& gResource{ get(group) };
			size_t size{ group.data().size() };
			if (size > gResource.capacity) {
				size_t newSize{ static_cast<size_t>(group.data().size() * capacityMultiplier) };
				gResource.capacity = newSize;
				OpenGL::bufferData(gResource.renderBuffers[1], group.data(), newSize, false);
			}
			else {
				OpenGL::subBufferData(gResource.renderBuffers[1], group.data());
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
			for (auto& [assetID, gResource] : _data) {
				std::visit([](auto& res) {
					RenderAPI::release(res);
					}, gResource);
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
		void set(const Shader& shader, const Tag& tag, const Type& value) {
			GPUResourceType<Shader>& gShader{ _shaders.at(shader.assetID()) };
			int64_t loc{ uniformLocation(gShader, tag) };
			RenderAPI::uniform(loc, value);
		}

		template<>
		void set(const Shader& shader, const Tag& tag, const TextureUnit& unit) {
			GPUResourceType<Shader>& gShader{ _shaders.at(shader.assetID()) };
			int64_t loc{ uniformLocation(gShader, tag) };
			RenderAPI::uniform(loc, static_cast<int>(unit));
		}

		void set(const Shader& shader, const Transform& transform) {
			GPUResourceType<Shader>& gShader{ _shaders.at(shader.assetID()) };
			RenderAPI::uniform(uniformLocation(gShader, "uPosition"), transform.position());
			RenderAPI::uniform(uniformLocation(gShader, "uScale"), transform.scale());
			RenderAPI::uniform(uniformLocation(gShader, "uRotation"), transform.rotation());
		}

		void set(const Shader& shader, const Material& material, const Repository& repository) {
			GPUResourceType<Shader>& gShader{ _shaders.at(shader.assetID()) };

			if (shader.useDefaultMaterial()) {
				int32_t materialMode{};
				constexpr size_t ALBEDO_BIT{ 0 };
				constexpr size_t MATERIAL_BIT{ 1 };

				if (material.albedoTexture() != 0) {
					materialMode |= (1 << ALBEDO_BIT);
					const Texture& texture{ repository.texture(material.albedoTexture()) };
					_memory->bind(texture, TextureUnit::UNIT_0);
					set(shader, "uAlbedoTexture", TextureUnit::UNIT_0);
				}
				else {
					int64_t loc{ uniformLocation(gShader, "uAlbedo") };
					RenderAPI::uniform(loc, material.color());
				}

				if (material.materialTexture() != 0) {
					materialMode |= (1 << MATERIAL_BIT);
					const Texture& texture{ repository.texture(material.materialTexture()) };
					TextureUnit textureUnit{ static_cast<TextureUnit>(materialMode) };
					_memory->bind(texture, textureUnit);
					set(shader, "uMaterialTexture", textureUnit);
				}
				else {
					RenderAPI::uniform(uniformLocation(gShader, "uMetallic"), material.metallic());
					RenderAPI::uniform(uniformLocation(gShader, "uRoughness"), material.roughness());
					RenderAPI::uniform(uniformLocation(gShader, "uEmission"), material.emission());
					RenderAPI::uniform(uniformLocation(gShader, "uAO"), material.ambientOcclusion());
				}

				RenderAPI::uniform(uniformLocation(gShader, "uMaterialMode"), materialMode);
			}

			for (const auto& [tag, input] : material.parameters()) {
				if (shader.uniforms().contains(tag)) {
					std::visit([this, &tag, &gShader](const auto& inputValue) {
						int64_t loc{ uniformLocation(gShader, tag) };
						RenderAPI::uniform(loc, inputValue);
						}, input);
				}
			}
		}

		void bind(const Shader& shader) const {
			const GPUResourceType<Shader>& gShader{ _shaders.at(shader.assetID()) };
			RenderAPI::bind(gShader);
		}

		void build(Shader& shader) {
			GPUResourceType<Shader> gShader{ RenderAPI::build(shader) };
			_shaders.emplace(shader.assetID(), std::move(gShader));
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
			for (auto& [assetID, gShader] : _shaders) {
				RenderAPI::release(gShader);
			}
			_shaders.clear();
		}

	private:
		int64_t uniformLocation(GPUResourceType<Shader>& gShader, const Tag& tag) const {
			auto value{ gShader.uniformCache.find(tag) };

			if (value != gShader.uniformCache.end()) {
				return value->second;
			}

			int64_t loc{ RenderAPI::uniformLocation(gShader, tag) };
			gShader.uniformCache.emplace(tag, loc);
			return loc;
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
					GPUResourceType<Texture>& gTexture{ _memory->get(texture) };
					ids.push_back(gTexture.id);

					texture.width(static_cast<size_t>(static_cast<float>(width) * buffer.resizeFactor()));
					texture.height(static_cast<size_t>(static_cast<float>(height) * buffer.resizeFactor()));
					_memory->data().erase(texture.assetID());
				}

				GPUResourceType<Framebuffer>& gBuffer{ _framebuffers.at(assetID) };
				RenderAPI::release(gBuffer, ids);
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
			const GPUResourceType<Framebuffer>& gBuffer{ _framebuffers.at(buffer.assetID()) };
			RenderAPI::bind(buffer, gBuffer);
		}

		void build(Framebuffer& buffer) {
			auto [gBuffer, textures] = RenderAPI::build(buffer);

			for (auto [assetID, gTexture] : textures) {
				_memory->data().emplace(assetID, gTexture);
				gBuffer.textures.push_back(assetID);
			}

			_framebuffers.emplace(buffer.assetID(), std::move(gBuffer));
		}

		bool built(const Framebuffer& buffer) const {
			return _framebuffers.contains(buffer.assetID());
		}

		void release(const Framebuffer& buffer) {
			Vector<GPUResourceID> ids;
			for (auto& [_, texture] : buffer.textures()) {
				GPUResourceType<Texture>& gTexture{ _memory->get(texture) };
				ids.push_back(gTexture.id);
				_memory->data().erase(texture.assetID());
			}

			GPUResourceType<Framebuffer>& gBuffer{ _framebuffers.at(buffer.assetID()) };
			RenderAPI::release(gBuffer, ids);
			_framebuffers.erase(buffer.assetID());
		}

		void clear() {
			for (auto& [assetID, gBuffer] : _framebuffers) {
				Vector<GPUResourceID> textures;

				for (auto assetID : gBuffer.textures) {
					if (_memory->data().contains(assetID)) {
						GPUResourceType<Texture>& gTexture{ 
							std::get<GPUResourceType<Texture>>(_memory->data().at(assetID)) };
						textures.push_back(gTexture.id);
						_memory->data().erase(assetID);
					}
				}

				RenderAPI::release(gBuffer, textures);
			}
		}
	};

}
