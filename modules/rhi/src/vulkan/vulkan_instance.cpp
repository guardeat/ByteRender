#include "vulkan/vulkan_instance.hpp"

#include "vulkan/vulkan_loader.hpp"
#include "vulkan/vulkan_requirements.hpp"

#include <bytee/core/assert.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace bytee::render::rhi {

namespace {

constexpr const char* k_validation_layer = "VK_LAYER_KHRONOS_validation";

// Window-system surface extensions, enabled when available. Literal names: no platform headers needed.
constexpr std::array k_platform_surface_extensions{
    "VK_EXT_metal_surface",    // macOS (MoltenVK)
    "VK_KHR_win32_surface",    // Windows
    "VK_KHR_xlib_surface",     // Linux X11
    "VK_KHR_xcb_surface",      // Linux X11 (XCB)
    "VK_KHR_wayland_surface",  // Linux Wayland
    "VK_EXT_headless_surface", // any platform: presentation without a window (tests, CI)
};

// volk's instance-level function pointers are process-global, so only one Instance may be alive (architecture §10).
// Part of the engine's single global-state exception.
std::atomic<bool> instance_alive{false};

bool has_layer(const std::vector<VkLayerProperties>& layers, const char* name) {
    return std::ranges::any_of(
        layers, [name](const VkLayerProperties& layer) { return std::strcmp(layer.layerName, name) == 0; });
}

core::LogLevel to_log_level(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types) {
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        return core::LogLevel::Error;
    }
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        // Warnings of type "general" only are loader housekeeping (e.g. manifest notes), not API misuse.
        return types == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT ? core::LogLevel::Debug : core::LogLevel::Warning;
    }
    return core::LogLevel::Trace; // info/verbose: loader chatter
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                              VkDebugUtilsMessageTypeFlagsEXT types,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
    auto* logger = static_cast<core::Logger*>(user_data);
    const std::string_view message = (data != nullptr && data->pMessage != nullptr) ? data->pMessage : "";
    logger->write(to_log_level(severity, types), message);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT make_messenger_info(core::Logger& logger) {
    return VkDebugUtilsMessengerCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
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
        if ((memory.memoryHeaps[heap].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
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

detail::InstanceImpl::~InstanceImpl() {
    if (debug_messenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance_alive.store(false, std::memory_order_release);
    }
}

detail::InstanceImpl* get_impl(const Instance& instance) noexcept {
    return instance.impl_;
}

void destroy(detail::InstanceImpl* impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    if (impl->live_devices.count() > 0) {
        impl->live_devices.report_leaks(*impl->logger, "Instance");
        BYTEE_ASSERT(false, "every Device must be destroyed before its Instance");
        return; // leaked on purpose: a leak is recoverable, a use-after-free is not
    }
    delete impl;
}

Instance::~Instance() {
    destroy(impl_);
}

Instance& Instance::operator=(Instance&& other) noexcept {
    if (this != &other) {
        destroy(impl_);
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}

bool Instance::is_validation_enabled() const noexcept {
    BYTEE_ASSERT(is_valid(), "Instance is empty");
    return impl_->validation_enabled;
}

bool Instance::is_presentation_supported() const noexcept {
    BYTEE_ASSERT(is_valid(), "Instance is empty");
    return impl_->presentation_supported;
}

std::span<const AdapterInfo> Instance::adapters() const noexcept {
    BYTEE_ASSERT(is_valid(), "Instance is empty");
    return impl_->adapters;
}

core::Result<Instance> InstanceFactory::create_instance(const InstanceDesc& desc) const {
    core::Logger& logger = *logger_;

    BYTEE_TRY(vulkan::load_vulkan_loader());

    uint32_t loader_version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion != nullptr) { // Vulkan 1.0 loaders lack it
        vkEnumerateInstanceVersion(&loader_version);
    }
    if (loader_version < vulkan::k_required_api_version) {
        const core::Version found = vulkan::to_version(loader_version);
        return core::make_error(core::ErrorCode::NotSupported, "Vulkan 1.3 is required, the loader supports {}.{}",
                                found.major, found.minor);
    }

    if (bool expected = false; !instance_alive.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return core::make_error(core::ErrorCode::NotSupported, "only one rhi::Instance may be alive at a time");
    }

    // From here on, the InstanceImpl destructor (or this guard, until vkCreateInstance succeeds) releases the slot.
    auto impl = std::make_unique<detail::InstanceImpl>();
    impl->logger = logger_;
    struct AliveGuard {
        const detail::InstanceImpl* impl;
        ~AliveGuard() {
            if (impl->instance == VK_NULL_HANDLE) {
                instance_alive.store(false, std::memory_order_release);
            }
        }
    } alive_guard{impl.get()};

    const auto layers = vulkan::enumerate<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
    const auto extensions = vulkan::enumerate<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);

    std::vector<const char*> enabled_layers;
    std::vector<const char*> enabled_extensions;
    VkInstanceCreateFlags flags = 0;

    // Non-conformant implementations (MoltenVK) are only listed with portability enumeration.
    if (vulkan::has_extension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        enabled_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Presentation: VK_KHR_surface plus every available platform surface extension.
    if (vulkan::has_extension(extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
        for (const char* platform_extension : k_platform_surface_extensions) {
            if (vulkan::has_extension(extensions, platform_extension)) {
                enabled_extensions.push_back(platform_extension);
                impl->presentation_supported = true;
            }
        }
        if (impl->presentation_supported) {
            enabled_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }
    }

    if (desc.enable_validation) {
        if (has_layer(layers, k_validation_layer) &&
            vulkan::has_extension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            enabled_layers.push_back(k_validation_layer);
            enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            impl->validation_enabled = true;
        } else {
            logger.warning("validation requested, but {} or {} is not installed; continuing without validation",
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
        .pEngineName = "Bytee",
        .engineVersion = VK_MAKE_API_VERSION(0, engine_version.major, engine_version.minor, engine_version.patch),
        .apiVersion = vulkan::k_required_api_version,
    };

    // Chained into instance creation so messages from vkCreateInstance/vkDestroyInstance are reported as well.
    const VkDebugUtilsMessengerCreateInfoEXT messenger_info = make_messenger_info(logger);

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = impl->validation_enabled ? &messenger_info : nullptr,
        .flags = flags,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(enabled_layers.size()),
        .ppEnabledLayerNames = enabled_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.data(),
    };

    // Created into locals: Vulkan leaves output handles undefined on failure (and the guard checks impl->instance).
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    if (result == VK_ERROR_LAYER_NOT_PRESENT && impl->validation_enabled) {
        // The layer is listed but its library failed to load (broken installation). Validation is optional.
        logger.warning("{} failed to load; continuing without validation", k_validation_layer);
        impl->validation_enabled = false;
        std::erase(enabled_layers, k_validation_layer);
        std::erase(enabled_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        create_info.pNext = nullptr;
        create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
        create_info.ppEnabledLayerNames = enabled_layers.data();
        create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
        create_info.ppEnabledExtensionNames = enabled_extensions.data();
        result = vkCreateInstance(&create_info, nullptr, &instance);
    }
    if (result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "vkCreateInstance");
    }
    impl->instance = instance;
    volkLoadInstanceOnly(impl->instance);

    if (impl->validation_enabled) {
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        result = vkCreateDebugUtilsMessengerEXT(impl->instance, &messenger_info, nullptr, &messenger);
        if (result != VK_SUCCESS) {
            return vulkan::make_vk_error(result, "vkCreateDebugUtilsMessengerEXT");
        }
        impl->debug_messenger = messenger;
    }

    impl->physical_devices = vulkan::enumerate<VkPhysicalDevice>(vkEnumeratePhysicalDevices, impl->instance);
    impl->adapters.reserve(impl->physical_devices.size());
    for (uint32_t index = 0; index < impl->physical_devices.size(); ++index) {
        impl->adapters.push_back(describe_adapter(impl->physical_devices[index], index));
    }

    const core::Version api = vulkan::to_version(loader_version);
    logger.debug("Vulkan instance created (loader {}.{}.{}, validation {}, presentation {}, {} adapter(s))", api.major,
                 api.minor, api.patch, impl->validation_enabled ? "on" : "off",
                 impl->presentation_supported ? "on" : "off", impl->adapters.size());
    return Instance{impl.release()};
}

} // namespace bytee::render::rhi
