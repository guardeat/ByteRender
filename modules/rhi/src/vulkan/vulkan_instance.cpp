#include "vulkan/vulkan_instance.hpp"

#include "vulkan/vulkan_device_requirements.hpp"
#include "vulkan/vulkan_loader.hpp"

#include <byte/core/assert.hpp>
#include <byte/render/rhi/instance_factory.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>

namespace byte::render::rhi {

namespace {

constexpr const char* k_validation_layer = "VK_LAYER_KHRONOS_validation";

// Window-system surface extensions, enabled when available. Names as literals so no platform headers are needed.
constexpr std::array k_platform_surface_extensions{
    "VK_EXT_metal_surface",   // macOS / iOS (MoltenVK)
    "VK_KHR_win32_surface",   // Windows
    "VK_KHR_xlib_surface",    // Linux X11
    "VK_KHR_xcb_surface",     // Linux X11 (XCB)
    "VK_KHR_wayland_surface", // Linux Wayland
};
using vulkan::k_required_api_version;

// Vulkan instance-level function pointers (volk) are process-global, so only one Instance may be alive.
std::atomic<bool> g_instance_alive{false};

bool has_layer(const std::vector<VkLayerProperties>& layers, const char* name) {
    return std::ranges::any_of(layers, [name](const VkLayerProperties& layer) {
        return std::strcmp(layer.layerName, name) == 0;
    });
}

bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::ranges::any_of(extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

core::LogLevel to_log_level(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        return core::LogLevel::Error;
    }
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        // "General" warnings are loader housekeeping (e.g. duplicate layer manifests), not API misuse.
        const bool is_general_only = types == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        return is_general_only ? core::LogLevel::Debug : core::LogLevel::Warning;
    }
    // Info/verbose messages are mostly loader chatter; keep them out of Debug output.
    return core::LogLevel::Trace;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT types,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    auto* logger = static_cast<core::Logger*>(user_data);
    const std::string_view message = (data && data->pMessage) ? data->pMessage : "";
    logger->write(to_log_level(severity, types), message);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT make_messenger_info(core::Logger& logger) {
    return VkDebugUtilsMessengerCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = &logger,
    };
}

AdapterType to_adapter_type(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return AdapterType::Discrete;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return AdapterType::Integrated;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return AdapterType::Virtual;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return AdapterType::Cpu;
    default: return AdapterType::Unknown;
    }
}

AdapterInfo describe_adapter(VkPhysicalDevice physical_device, uint32_t index) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    AdapterInfo info{
        .index = index,
        .name = properties.deviceName,
        .type = to_adapter_type(properties.deviceType),
        .vendor_id = properties.vendorID,
        .device_id = properties.deviceID,
        .api_version = vulkan::to_version(properties.apiVersion),
    };

    if (properties.apiVersion >= VK_API_VERSION_1_2) {
        VkPhysicalDeviceVulkan12Properties vulkan12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                                .pNext = &vulkan12};
        vkGetPhysicalDeviceProperties2(physical_device, &properties2);
        info.driver_name = vulkan12.driverName;
        info.driver_info = vulkan12.driverInfo;
    }

    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory);
    for (uint32_t heap = 0; heap < memory.memoryHeapCount; ++heap) {
        if (memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            info.device_local_memory_bytes += memory.memoryHeaps[heap].size;
        }
    }

    if (auto reason = vulkan::check_adapter_requirements(physical_device)) {
        info.unsupported_reason = std::move(*reason);
    } else {
        info.is_supported = true;
    }
    return info;
}

} // namespace

Instance::Impl::~Impl() {
    if (live_devices.count() > 0) {
        live_devices.report_leaks(*logger, "Instance");
        BYTE_RENDER_ASSERT(false, "all Devices must be destroyed before their Instance");
    }
    if (debug_messenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        g_instance_alive.store(false);
    }
}

Instance::~Instance() {
    delete m_impl;
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        delete m_impl;
        m_impl = std::exchange(other.m_impl, nullptr);
    }
    return *this;
}

bool Instance::is_validation_enabled() const noexcept {
    BYTE_RENDER_ASSERT(is_valid(), "Instance is empty");
    return m_impl->validation_enabled;
}

std::span<const AdapterInfo> Instance::adapters() const noexcept {
    BYTE_RENDER_ASSERT(is_valid(), "Instance is empty");
    return m_impl->adapters;
}

core::Result<Instance> InstanceFactory::create_instance(const InstanceDesc& desc) const {
    core::Logger& logger = *m_logger;

    if (auto loaded = vulkan::load_vulkan_loader(); !loaded) {
        return std::move(loaded).error();
    }

    uint32_t loader_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) {
        vkEnumerateInstanceVersion(&loader_version);
    }
    if (loader_version < k_required_api_version) {
        const core::Version found = vulkan::to_version(loader_version);
        return core::make_error(core::ErrorCode::NotSupported, "Vulkan 1.3 is required, the loader supports {}.{}",
                                found.major, found.minor);
    }

    bool expected = false;
    if (!g_instance_alive.compare_exchange_strong(expected, true)) {
        return core::make_error(core::ErrorCode::NotSupported, "only one rhi::Instance may exist at a time");
    }

    const auto layers = vulkan::enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
    const auto extensions =
        vulkan::enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    std::vector<const char*> enabled_layers;
    std::vector<const char*> enabled_extensions;
    VkInstanceCreateFlags flags = 0;

    // Non-conformant implementations (MoltenVK on macOS) are only listed with portability enumeration.
    if (has_extension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        enabled_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Presentation support: VK_KHR_surface plus at least one platform surface extension.
    bool surface_supported = false;
    if (has_extension(extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
        for (const char* platform_extension : k_platform_surface_extensions) {
            if (has_extension(extensions, platform_extension)) {
                enabled_extensions.push_back(platform_extension);
                surface_supported = true;
            }
        }
        if (surface_supported) {
            enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }
    }

    bool validation = false;
    if (desc.enable_validation) {
        if (has_layer(layers, k_validation_layer) && has_extension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            enabled_layers.push_back(k_validation_layer);
            enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validation = true;
        } else {
            logger.warning("validation requested, but {} or {} is not available; continuing without it",
                           k_validation_layer, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }

    const std::string app_name{desc.app_name};
    const core::Version engine_version = core::version();
    const VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = app_name.c_str(),
        .applicationVersion =
            VK_MAKE_API_VERSION(0, desc.app_version.major, desc.app_version.minor, desc.app_version.patch),
        .pEngineName = "ByteRender",
        .engineVersion = VK_MAKE_API_VERSION(0, engine_version.major, engine_version.minor, engine_version.patch),
        .apiVersion = k_required_api_version,
    };

    // Chained into instance creation so messages from vkCreateInstance/vkDestroyInstance are reported too.
    const VkDebugUtilsMessengerCreateInfoEXT messenger_info = make_messenger_info(logger);

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = validation ? &messenger_info : nullptr,
        .flags = flags,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
        .ppEnabledLayerNames = enabled_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.data(),
    };

    auto impl = std::make_unique<Instance::Impl>();
    impl->logger = m_logger;
    impl->validation_enabled = validation;
    impl->surface_supported = surface_supported;

    VkResult result = vkCreateInstance(&create_info, nullptr, &impl->instance);
    if (result == VK_ERROR_LAYER_NOT_PRESENT && validation) {
        // The layer is listed but its library failed to load (broken install). Validation is optional.
        logger.warning("{} failed to load; continuing without validation", k_validation_layer);
        validation = false;
        impl->validation_enabled = false;
        std::erase(enabled_layers, k_validation_layer);
        std::erase(enabled_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        create_info.pNext = nullptr;
        create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
        create_info.ppEnabledLayerNames = enabled_layers.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
        create_info.ppEnabledExtensionNames = enabled_extensions.data();
        result = vkCreateInstance(&create_info, nullptr, &impl->instance);
    }
    if (result != VK_SUCCESS) {
        g_instance_alive.store(false);
        return vulkan::make_vk_error(result, "vkCreateInstance");
    }
    volkLoadInstanceOnly(impl->instance);

    if (validation) {
        result = vkCreateDebugUtilsMessengerEXT(impl->instance, &messenger_info, nullptr, &impl->debug_messenger);
        if (result != VK_SUCCESS) {
            return vulkan::make_vk_error(result, "vkCreateDebugUtilsMessengerEXT");
        }
    }

    impl->physical_devices = vulkan::enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, impl->instance);
    impl->adapters.reserve(impl->physical_devices.size());
    for (uint32_t i = 0; i < impl->physical_devices.size(); ++i) {
        impl->adapters.push_back(describe_adapter(impl->physical_devices[i], i));
    }

    const core::Version api = vulkan::to_version(loader_version);
    logger.debug("Vulkan instance created (loader {}.{}.{}, validation {}, presentation {}, {} adapter(s))",
                 api.major, api.minor, api.patch, validation ? "on" : "off", surface_supported ? "on" : "off",
                 impl->adapters.size());

    return Instance{impl.release()};
}

} // namespace byte::render::rhi
