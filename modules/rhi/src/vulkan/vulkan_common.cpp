#include "vulkan/vulkan_common.hpp"

#include <algorithm>
#include <format>
#include <string>

namespace bytee::render::rhi::vulkan {

bool has_extension(std::span<const VkExtensionProperties> extensions, std::string_view name) noexcept {
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return std::string_view{extension.extensionName} == name;
    });
}

std::string_view to_string(VkResult result) noexcept {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    default: return "VkResult(unknown)";
    }
}

core::ErrorCode to_error_code(VkResult result) noexcept {
    switch (result) {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return core::ErrorCode::OutOfMemory;
    case VK_ERROR_LAYER_NOT_PRESENT:
    case VK_ERROR_EXTENSION_NOT_PRESENT:
    case VK_ERROR_FEATURE_NOT_PRESENT:
    case VK_ERROR_INCOMPATIBLE_DRIVER:
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return core::ErrorCode::NotSupported;
    case VK_ERROR_INITIALIZATION_FAILED: return core::ErrorCode::InitializationFailed;
    case VK_ERROR_DEVICE_LOST: return core::ErrorCode::DeviceLost;
    case VK_TIMEOUT: return core::ErrorCode::Timeout;
    default: return core::ErrorCode::Unknown;
    }
}

void set_debug_name(VkDevice device, VkObjectType type, uint64_t handle, std::string_view name) {
    if (vkSetDebugUtilsObjectNameEXT == nullptr || name.empty()) {
        return;
    }
    const std::string terminated{name};
    const VkDebugUtilsObjectNameInfoEXT info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = handle,
        .pObjectName = terminated.c_str(),
    };
    vkSetDebugUtilsObjectNameEXT(device, &info);
}

core::Error make_vk_error(VkResult result, core::ErrorFormat<> call) {
#if defined(BYTEE_ENABLE_ERROR_LOCATIONS)
    return core::Error{to_error_code(result), std::format("{} failed: {}", std::format(call.format), to_string(result)),
                       call.location};
#else
    return core::Error{to_error_code(result), std::format("{} failed: {}", std::format(call), to_string(result))};
#endif
}

} // namespace bytee::render::rhi::vulkan
