#pragma once

#include <type_traits>
#include <stdexcept>

#include "core/core_types.h"
#include "core/asset.h"
#include "render_types.h"
#include "framebuffer.h"
#include "texture.h"
#include "instance_group.h"

namespace Byte {

	template<typename RenderAPI, typename... GPUTypes>
	class GPUMemoryDevice {
	private:
		using GPUVariant = Variant<GPUTypes...>;

		Map<AssetID, GPUVariant> _data;

	public:
		~GPUMemoryDevice() {
			clear();
		}

		template<typename AssetType>
		GPUResource<AssetType>& get(const AssetType& asset) {
			auto it{ _data.find(asset.assetID()) };
			if (it != _data.end()) {
				return std::get<GPUResource<AssetType>>(it->second);
			}
			else {
				throw std::runtime_error("Asset not found in gpu memory");
			}
		}

		template<typename AssetType>
		void load(AssetType& asset) {
			if constexpr (std::is_same_v<AssetType, Framebuffer>) {
				auto [id, textures] = RenderAPI::build(asset);

				for (auto [assetID, gID] : textures) {
					_data.emplace(assetID, gID);
				}

				_data.emplace(asset.assetID(), std::move(id));
			}
			else {
				auto gID{ RenderAPI::build(asset) };

				_data.emplace(asset.assetID(), std::move(gID));
			}
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
				if constexpr (std::is_same_v<AssetType, Framebuffer>) {
					Vector<GPUResourceID> ids;
					for (auto& [_, texture] : asset.textures()) {
						GPUResource<Texture>& textureID{ get(texture) };
						ids.push_back(textureID.id);
						_data.erase(texture.assetID());
					}

					RenderAPI::release(std::get<GPUResource<Framebuffer>>(it->second), ids);
					_data.erase(it);
				}
				else {
					std::visit([](auto& resource) {
						RenderAPI::release(resource);
						}, it->second);
					_data.erase(it);
				}
			}
		}

		template<typename AssetType>
		bool loaded(const AssetType& asset) const {
			return _data.contains(asset.assetID());
		}

		template<typename AssetType>
		void bind(const AssetType& asset) {
			GPUResource<AssetType>& resourceID{ get(asset) };

			if constexpr (std::is_same_v<AssetType, Framebuffer>) {
				RenderAPI::bind(asset, resourceID);
			}

			else if constexpr (std::is_same_v<AssetType, Texture>) {
				bind(asset, TextureUnit::UNIT_0);
			}

			else {
				RenderAPI::bind(resourceID);
			}
		}

		void bind(const Texture& texture, TextureUnit unit) {
			GPUResource<Texture> resourceID{ get(texture) };
			RenderAPI::bind(resourceID, unit);
		}

		void bind(size_t width, size_t height) {
			RenderAPI::bind(width, height);
		}

		void update(InstanceGroup& group, float capacityMultiplier = 2.0f) {
			GPUResource<InstanceGroup>& bufferGroup{ get(group) };
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

	template<typename RenderAPI, typename GPUMemoryDeviceType>
	class GPUUniformDevice {
	public:
		template<typename Type>
		void uniform(
			GPUMemoryDeviceType& memory, 
			const Shader& shader, 
			const Tag& tag,
			const Type& value) {
			GPUResource<Shader> id{ memory.get(shader) };
			RenderAPI::uniform(id, tag, value);
		}

		void uniform(
			GPUMemoryDeviceType& memory, 
			const Shader& shader, 
			const Material& material, 
			const Repository& repository) {
			GPUResource<Shader> id{ memory.get(shader) };

			if (shader.useDefaultMaterial()) {
				int32_t materialMode{};
				constexpr size_t ALBEDO_BIT{ 0 };
				constexpr size_t MATERIAL_BIT{ 1 };

				if( material.albedoTexture() != 0) {
					materialMode |= (1 << ALBEDO_BIT);
					const Texture& texture{ repository.texture(material.albedoTexture()) };
					uniform(memory, shader, "uAlbedoTexture", texture, TextureUnit::UNIT_0);
				}
				else {
					RenderAPI::uniform(id, "uAlbedo", material.color());
				}

				if( material.materialTexture() != 0) {
					materialMode |= (1 << MATERIAL_BIT);
					const Texture& texture{ repository.texture(material.materialTexture()) };
					TextureUnit textureUnit{ static_cast<TextureUnit>(materialMode) };
					uniform(memory,shader, "uMaterialTexture", texture, textureUnit);
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

		void uniform(GPUMemoryDeviceType& memory, const Shader& shader, const Transform& transform) {
			GPUResource<Shader> id{ memory.get(shader) };
			OpenGL::uniform(id, "uPosition", transform.position());
			OpenGL::uniform(id, "uScale", transform.scale());
			OpenGL::uniform(id, "uRotation", transform.rotation());
		}

		void uniform(
			GPUMemoryDeviceType& memory,
			const Shader& shader, 
			const Tag& uniform,
			const Texture& texture, 
			TextureUnit unit = TextureUnit::UNIT_0) {
			memory.bind(texture, unit);

			GPUResource<Shader> id{ memory.get(shader) };
			OpenGL::uniform(id, uniform, static_cast<int>(unit));
		}
		

	};

}
