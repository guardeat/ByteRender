#pragma once

#include <vector>
#include <memory>
#include <filesystem>
#include <initializer_list>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <variant>
#include <cstdint>

namespace Byte {

	template<typename Type>
	using Vector = std::vector<Type>;

	template<typename Type>
	using UniquePtr = std::unique_ptr<Type>;

	using Path = std::filesystem::path;

	template<typename Type>
	using InitializerList = std::initializer_list<Type>;

	template<typename Key, typename Value>
	using Map = std::unordered_map<Key, Value>;

	template<typename Value>
	using Set = std::unordered_set<Value>;

	template<typename... Args>
	using Variant = std::variant<Args...>;

	template<typename First, typename Second>
	using Pair = std::pair<First, Second>;

	using Tag = std::string;

	using AssetID = uint64_t;
}
