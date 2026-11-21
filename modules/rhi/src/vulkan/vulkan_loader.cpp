#include "vulkan/vulkan_loader.hpp"

#include "vulkan/vulkan_common.hpp"

#if defined(__APPLE__)
    #include <cstdlib>
    #include <dlfcn.h>
    #include <string>
#endif

namespace bytee::render::rhi::vulkan {

namespace {

#if defined(__APPLE__)
bool load_from_path(const char* path) {
    void* module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) {
        return false;
    }
    auto* get_instance_proc_addr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(module, "vkGetInstanceProcAddr"));
    if (get_instance_proc_addr == nullptr) {
        dlclose(module);
        return false;
    }
    volkInitializeCustom(get_instance_proc_addr);
    return true;
}

// volk searches only the default dyld paths and /usr/local/lib. Look first next to the application (shipped apps
// bundle the loader), then let volk try, then $VULKAN_SDK and Homebrew's prefix (Apple Silicon).
VkResult initialize_loader() {
    for (const char* bundled :
         {"@executable_path/libvulkan.1.dylib", "@executable_path/../Frameworks/libvulkan.1.dylib"}) {
        if (load_from_path(bundled)) {
            return VK_SUCCESS;
        }
    }
    if (volkInitialize() == VK_SUCCESS) {
        return VK_SUCCESS;
    }
    if (const char* sdk = std::getenv("VULKAN_SDK"); sdk != nullptr) {
        const std::string path = std::string{sdk} + "/lib/libvulkan.1.dylib";
        if (load_from_path(path.c_str())) {
            return VK_SUCCESS;
        }
    }
    for (const char* system : {"/opt/homebrew/lib/libvulkan.1.dylib", "/usr/local/lib/libvulkan.1.dylib"}) {
        if (load_from_path(system)) {
            return VK_SUCCESS;
        }
    }
    return VK_ERROR_INITIALIZATION_FAILED;
}
#else
VkResult initialize_loader() {
    return volkInitialize();
}
#endif

} // namespace

core::Result<void> load_vulkan_loader() {
    // The loader is process-wide by nature (architecture §10). Thread-safe one-time initialization.
    static const VkResult result = initialize_loader();
    if (result != VK_SUCCESS) {
        return core::make_error(core::ErrorCode::NotSupported,
                                "Vulkan loader not found: install the Vulkan SDK or a Vulkan driver");
    }
    return {};
}

} // namespace bytee::render::rhi::vulkan
