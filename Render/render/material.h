#pragma once

#include <string>

#include "core/byte_math.h"
#include "core/uid_generator.h"
#include "render_types.h"

namespace Byte {

	class Material : public Asset {
	private:
		float _metallic{ 0.0f };
		float _roughness{ 0.9f };
		float _emission{ 0.0f };
		float _ambientOcclusion{ 0.5f };

		Vec4 _color{ 1.0f, 1.0f, 1.0f, 1.0f };

		TransparencyMode _transparency{ TransparencyMode::OPAQUE };

		Map<Tag, AssetID> _shaders;
		Map<Tag, AssetID> _textures;

		using ParameterMap = Map<Tag, Variant<bool, int, uint64_t, float, Vec3, Quaternion>>;
		ParameterMap _parameters;

	public:
		float metallic() const {
			return _metallic;
		}

		void metallic(float value) {
			_metallic = value;
		}

		float roughness() const {
			return _roughness;
		}

		void roughness(float value) {
			_roughness = value;
		}

		float emission() const {
			return _emission;
		}

		void emission(float value) {
			_emission = value;
		}

		float ambientOcclusion() const {
			return _ambientOcclusion;
		}

		void ambientOcclusion(float value) {
			_ambientOcclusion = value;
		}

		const Vec4& color() const {
			return _color;
		}

		void color(const Vec4& value) {
			_color = value;
		}

		void color(const Vec3& value) {
			_color = Vec4{ value.x, value.y, value.z, 1.0f };
		}

		TransparencyMode transparency() const {
			return _transparency;
		}

		void transparency(TransparencyMode mode) {
			_transparency = mode;
		}

		void shader(const Tag& tag, AssetID shader) {
			_shaders[tag] = shader;
		}

		AssetID shader(const Tag& tag) const {
			auto it{ _shaders.find(tag) };
			return it != _shaders.end() ? it->second : 0;
		}

		void texture(const Tag& tag, AssetID texture) {
			_textures[tag] = texture;
		}

		AssetID texture(const Tag& tag) const {
			auto it{ _textures.find(tag) };
			return it != _textures.end() ? it->second : 0;
		}

		template<typename Type>
		void parameter(const Tag& tag, const Type& value) {
			_parameters[tag] = value;
		}

		template<typename Type>
		Type& parameter(const Tag& tag) {
			return std::get<Type>(_parameters.at(tag));
		}

		template<typename Type>
		const Type& parameter(const Tag& tag) const {
			return std::get<Type>(_parameters.at(tag));
		}

		ParameterMap& parameters() {
			return _parameters;
		}

		const ParameterMap& parameters() const {
			return _parameters;
		}

		bool hasShader(const Tag& tag) const {
			return _shaders.contains(tag);
		}

		bool hasTexture(const Tag& tag) const {
			return _textures.contains(tag);
		}

		bool hasParameter(const Tag& tag) const {
			return _parameters.contains(tag);
		}

		Map<Tag, AssetID>& shaders() {
			return _shaders;
		}

		const Map<Tag, AssetID>& shaders() const {
			return _shaders;
		}

		Map<Tag, AssetID>& textures() {
			return _textures;
		}

		const Map<Tag, AssetID>& textures() const {
			return _textures;
		}
	};

}
