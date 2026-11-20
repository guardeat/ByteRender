#include "vulkan/vulkan_common.hpp"

namespace byte::render::rhi::vulkan {

std::string_view to_string(VkResult result) noexcept {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
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
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
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

core::Error make_vk_error(VkResult result, std::string_view call) {
    return core::make_error(to_error_code(result), "{} failed: {}", call, to_string(result));
}

} // namespace byte::render::rhi::vulkan
