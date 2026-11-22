// Swapchains: surfaces from NativeWindow, presentable images as external textures, frames in flight, recreation.

#include "vulkan/vulkan_convert.hpp"
#include "vulkan/vulkan_device.hpp"
#if defined(__linux__)
    #include "vulkan/vulkan_surface_linux.hpp"
#endif

#include <bytee/core/assert.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <string>
#include <vector>

namespace bytee::render::rhi {

struct BYTEE_HIDDEN detail::SwapchainImpl {
    detail::DeviceImpl* device = nullptr;
    detail::LiveObjectTracker::Id tracker_id = 0;
    std::string debug_name;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surface_format{};
    Format format = Format::Undefined;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    core::Extent2D extent;           // current images
    core::Extent2D requested_extent; // from create / resize()
    bool needs_recreate = false;
    std::vector<Format> preferred_formats; // kept for creations that happen later (e.g. window had no size yet)
    PresentMode preferred_present_mode = PresentMode::Fifo;

    std::vector<TextureHandle> images;           // external textures, one per swapchain image
    std::vector<VkSemaphore> present_semaphores; // per image: rendering done -> present
    std::vector<VkSemaphore> acquire_semaphores; // per frame slot: image acquired -> rendering
    std::vector<SyncPoint> frame_points;         // per frame slot: last submission using the slot
    uint32_t frame_slot = 0;
    uint32_t acquired_image = std::numeric_limits<uint32_t>::max();

    SwapchainImpl() = default;
    ~SwapchainImpl();
    SwapchainImpl(const SwapchainImpl&) = delete;
    SwapchainImpl& operator=(const SwapchainImpl&) = delete;

    [[nodiscard]] core::Result<void> create_swapchain();
    [[nodiscard]] core::Result<void> recreate();
    void destroy_images_and_semaphores() noexcept;
};

namespace {

constexpr std::array k_default_formats{Format::BGRA8Srgb, Format::RGBA8Srgb, Format::BGRA8Unorm};

VkPresentModeKHR to_vk_present_mode(PresentMode mode) noexcept {
    switch (mode) {
    case PresentMode::Fifo: return VK_PRESENT_MODE_FIFO_KHR;
    case PresentMode::Mailbox: return VK_PRESENT_MODE_MAILBOX_KHR;
    case PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

core::Result<VkSurfaceKHR> create_surface(VkInstance instance, const NativeWindow& window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = VK_ERROR_EXTENSION_NOT_PRESENT;
    switch (window.platform) {
    case NativeWindow::Platform::Headless:
        if (vkCreateHeadlessSurfaceEXT != nullptr) {
            const VkHeadlessSurfaceCreateInfoEXT info{.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
            result = vkCreateHeadlessSurfaceEXT(instance, &info, nullptr, &surface);
        }
        break;
    case NativeWindow::Platform::Metal:
#if defined(VK_USE_PLATFORM_METAL_EXT)
        if (vkCreateMetalSurfaceEXT != nullptr) {
            const VkMetalSurfaceCreateInfoEXT info{.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
                                                   .pLayer = static_cast<const CAMetalLayer*>(window.handle)};
            result = vkCreateMetalSurfaceEXT(instance, &info, nullptr, &surface);
        }
#endif
        break;
    case NativeWindow::Platform::Win32:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        if (vkCreateWin32SurfaceKHR != nullptr) {
            const VkWin32SurfaceCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                                                   .hinstance = static_cast<HINSTANCE>(window.display),
                                                   .hwnd = static_cast<HWND>(window.handle)};
            result = vkCreateWin32SurfaceKHR(instance, &info, nullptr, &surface);
        }
#endif
        break;
    case NativeWindow::Platform::Xlib:
    case NativeWindow::Platform::Xcb:
    case NativeWindow::Platform::Wayland:
#if defined(__linux__)
        result = vulkan::create_linux_surface(instance, window, surface);
#endif
        break;
    }
    if (result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        return core::make_error(core::ErrorCode::NotSupported,
                                "this window platform is not supported by this build or GPU driver");
    }
    if (result != VK_SUCCESS) {
        return vulkan::make_vk_error(result, "surface creation");
    }
    return surface;
}

} // namespace

detail::SwapchainImpl::~SwapchainImpl() {
    if (device == nullptr) {
        return;
    }
    // Swapchain teardown is rare: waiting for the GPU keeps the presentation semaphores and images safe to free.
    static_cast<void>(device->vk.vkDeviceWaitIdle(device->device));
    destroy_images_and_semaphores();
    for (const VkSemaphore semaphore : acquire_semaphores) {
        device->vk.vkDestroySemaphore(device->device, semaphore, nullptr);
    }
    if (swapchain != VK_NULL_HANDLE) {
        device->vk.vkDestroySwapchainKHR(device->device, swapchain, nullptr);
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(device->instance->instance, surface, nullptr);
    }
    if (tracker_id != 0) {
        device->live_objects.remove(tracker_id);
    }
}

void detail::SwapchainImpl::destroy_images_and_semaphores() noexcept {
    for (const TextureHandle image : images) {
        device->destroy_now(image); // only views: the images belong to the swapchain
    }
    images.clear();
    for (const VkSemaphore semaphore : present_semaphores) {
        device->vk.vkDestroySemaphore(device->device, semaphore, nullptr);
    }
    present_semaphores.clear();
}

core::Result<void> detail::SwapchainImpl::create_swapchain() {
    detail::DeviceImpl& impl = *device;
    VkSurfaceCapabilitiesKHR capabilities{};
    if (const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(impl.physical_device, surface, &capabilities);
        result != VK_SUCCESS) {
        return impl.fail(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    }

    if (format == Format::Undefined) { // first creation: choose format and present mode once
        const auto formats =
            vulkan::enumerate<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, impl.physical_device, surface);
        const std::span<const Format> candidates =
            preferred_formats.empty() ? std::span<const Format>{k_default_formats} : preferred_formats;
        for (const Format candidate : candidates) {
            const VkFormat vk_format = vulkan::to_vk_format(candidate);
            const auto match = std::ranges::find_if(formats, [vk_format](const VkSurfaceFormatKHR& available) {
                return available.format == vk_format && available.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            });
            if (match != formats.end()) {
                surface_format = *match;
                format = candidate;
                break;
            }
        }
        if (format == Format::Undefined) {
            return core::make_error(core::ErrorCode::NotSupported,
                                    "swapchain '{}': none of the preferred formats is supported by the surface",
                                    debug_name);
        }
        const auto modes = vulkan::enumerate<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR,
                                                               impl.physical_device, surface);
        const VkPresentModeKHR wanted = to_vk_present_mode(preferred_present_mode);
        present_mode = std::ranges::find(modes, wanted) != modes.end() ? wanted : VK_PRESENT_MODE_FIFO_KHR;
    }

    // A defined currentExtent must be used as is; otherwise the window size decides, within the surface limits.
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = {.width = capabilities.currentExtent.width, .height = capabilities.currentExtent.height};
    } else {
        extent = {.width = std::clamp(requested_extent.width, capabilities.minImageExtent.width,
                                      capabilities.maxImageExtent.width),
                  .height = std::clamp(requested_extent.height, capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height)};
    }

    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount != 0) {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }
    constexpr VkImageUsageFlags k_wanted_usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    const VkImageUsageFlags usage = capabilities.supportedUsageFlags & k_wanted_usage;
    if ((usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        return core::make_error(core::ErrorCode::NotSupported, "swapchain '{}': images cannot be render targets",
                                debug_name);
    }
    const VkCompositeAlphaFlagBitsKHR composite =
        (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0
            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            : static_cast<VkCompositeAlphaFlagBitsKHR>(capabilities.supportedCompositeAlpha &
                                                       ~(capabilities.supportedCompositeAlpha - 1)); // lowest bit

    const VkSwapchainKHR old_swapchain = swapchain;
    const VkSwapchainCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = {.width = extent.width, .height = extent.height},
        .imageArrayLayers = 1,
        .imageUsage = usage,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = composite,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };
    if (const VkResult result = impl.vk.vkCreateSwapchainKHR(impl.device, &info, nullptr, &swapchain);
        result != VK_SUCCESS) {
        swapchain = old_swapchain;
        return impl.fail(result, "vkCreateSwapchainKHR");
    }
    if (old_swapchain != VK_NULL_HANDLE) {
        impl.vk.vkDestroySwapchainKHR(impl.device, old_swapchain, nullptr); // the GPU is idle (see recreate())
    }
    vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, swapchain, debug_name);

    // Images become external textures (views only), so they work with barriers, render passes and copies.
    core::Flags<TextureUsage> texture_usage = TextureUsage::ColorAttachment;
    if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0) {
        texture_usage |= TextureUsage::CopyDst;
    }
    if ((usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0) {
        texture_usage |= TextureUsage::CopySrc;
    }
    const auto vk_images = vulkan::enumerate<VkImage>(impl.vk.vkGetSwapchainImagesKHR, impl.device, swapchain);
    for (std::size_t index = 0; index < vk_images.size(); ++index) {
        const std::string image_name = std::format("{} image {}", debug_name, index);
        vulkan::TextureRecord record;
        record.image = vk_images[index];
        record.external = true;
        const VkImageViewCreateInfo view_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = record.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surface_format.format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
        };
        if (const VkResult result = impl.vk.vkCreateImageView(impl.device, &view_info, nullptr, &record.view);
            result != VK_SUCCESS) {
            return impl.fail(result, "vkCreateImageView (swapchain)");
        }
        record.attachment_view = record.view;
        record.info = TextureInfo{.extent = {.width = extent.width, .height = extent.height},
                                  .format = format,
                                  .usage = texture_usage,
                                  .debug_name = image_name};
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_IMAGE, record.image, image_name);
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_IMAGE_VIEW, record.view, std::format("{} view", image_name));

        const VkImageView view = record.view;
        TextureHandle handle;
        {
            const std::scoped_lock lock{impl.resources.mutex};
            record.tracker_id = impl.live_objects.add("Texture", image_name);
            handle = impl.resources.textures.insert(std::move(record)); // a full pool returns before consuming `record`
            if (!handle.is_valid()) {
                impl.live_objects.remove(record.tracker_id);
            }
        }
        if (!handle.is_valid()) {
            impl.vk.vkDestroyImageView(impl.device, view, nullptr);
            return core::make_error(core::ErrorCode::LimitExceeded,
                                    "swapchain '{}': the device's texture capacity ({}) is reached", debug_name,
                                    impl.resources.textures.capacity());
        }
        images.push_back(handle);

        VkSemaphore semaphore = VK_NULL_HANDLE;
        const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (const VkResult result = impl.vk.vkCreateSemaphore(impl.device, &semaphore_info, nullptr, &semaphore);
            result != VK_SUCCESS) {
            return impl.fail(result, "vkCreateSemaphore (present)");
        }
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_SEMAPHORE, semaphore, std::format("{} present", image_name));
        present_semaphores.push_back(semaphore);
    }
    needs_recreate = false;
    return {};
}

core::Result<void> detail::SwapchainImpl::recreate() {
    // Rare (resize, out of date): waiting for the GPU makes every old image and semaphore safe to replace.
    if (const VkResult result = device->vk.vkDeviceWaitIdle(device->device); result != VK_SUCCESS) {
        return device->fail(result, "vkDeviceWaitIdle");
    }
    destroy_images_and_semaphores();
    std::ranges::fill(frame_points, SyncPoint{});
    acquired_image = std::numeric_limits<uint32_t>::max();
    needs_recreate = true;     // stays set if creation fails halfway, so a later acquire() retries instead of using it
    return create_swapchain(); // keeps the format and present mode chosen at the first creation
}

// ----------------------------------------------------------------------------------------------------------------
// Public API

detail::SwapchainImpl* get_impl(const Swapchain& swapchain) noexcept {
    return swapchain.impl_;
}

Swapchain::~Swapchain() {
    delete impl_;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}

core::Extent2D Swapchain::extent() const noexcept {
    BYTEE_ASSERT(is_valid(), "Swapchain is empty");
    return impl_->extent;
}

Format Swapchain::format() const noexcept {
    BYTEE_ASSERT(is_valid(), "Swapchain is empty");
    return impl_->format;
}

uint32_t Swapchain::image_count() const noexcept {
    BYTEE_ASSERT(is_valid(), "Swapchain is empty");
    return static_cast<uint32_t>(impl_->images.size());
}

void Swapchain::resize(core::Extent2D extent) noexcept {
    BYTEE_ASSERT(is_valid(), "Swapchain is empty");
    if (extent != impl_->requested_extent) {
        impl_->requested_extent = extent;
        impl_->needs_recreate = true;
    }
}

core::Result<SwapchainFrame> Swapchain::acquire() {
    BYTEE_ASSERT(is_valid(), "Swapchain is empty");
    detail::SwapchainImpl& swapchain = *impl_;
    detail::DeviceImpl& impl = *swapchain.device;
    BYTEE_ASSERT(swapchain.acquired_image == std::numeric_limits<uint32_t>::max(),
                 "acquire() called twice without present()");
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    if (swapchain.requested_extent.width == 0 || swapchain.requested_extent.height == 0) {
        return SwapchainFrame{}; // minimized: nothing to render
    }
    if (swapchain.needs_recreate) {
        BYTEE_TRY(swapchain.recreate());
    }

    // Frames in flight: the slot's previous frame must be done before its acquire semaphore is reused.
    const SyncPoint previous = swapchain.frame_points[swapchain.frame_slot];
    if (const VkResult result = impl.queue(previous.queue).wait(previous.value, UINT64_MAX); result != VK_SUCCESS) {
        return impl.fail(result, "vkWaitSemaphores (frame in flight)");
    }

    for (int attempt = 0; attempt < 2; ++attempt) {
        uint32_t index = 0;
        const VkResult result =
            impl.vk.vkAcquireNextImageKHR(impl.device, swapchain.swapchain, UINT64_MAX,
                                          swapchain.acquire_semaphores[swapchain.frame_slot], VK_NULL_HANDLE, &index);
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
            swapchain.needs_recreate = result == VK_SUBOPTIMAL_KHR; // still usable this frame
            swapchain.acquired_image = index;
            return SwapchainFrame{.texture = swapchain.images[index], .image_index = index};
        }
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            BYTEE_TRY(swapchain.recreate());
            continue;
        }
        if (result == VK_ERROR_SURFACE_LOST_KHR) {
            return core::make_error(core::ErrorCode::NotSupported, "swapchain '{}': the surface was lost",
                                    swapchain.debug_name);
        }
        return impl.fail(result, "vkAcquireNextImageKHR");
    }
    return core::make_error(core::ErrorCode::Unknown, "swapchain '{}': still out of date after recreation",
                            swapchain.debug_name);
}

core::Result<Swapchain> Device::create_swapchain(const SwapchainDesc& desc) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    detail::DeviceImpl& impl = *impl_;
    if (impl.lost.load(std::memory_order_acquire)) {
        return impl.lost_error();
    }
    if (!impl.presentation_supported) {
        return core::make_error(core::ErrorCode::NotSupported, "device '{}' does not support presentation",
                                impl.debug_name);
    }
    if (desc.frames_in_flight == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "swapchain '{}': frames_in_flight must not be 0",
                                desc.debug_name);
    }

    auto swapchain = std::make_unique<detail::SwapchainImpl>();
    swapchain->device = &impl;
    swapchain->debug_name = desc.debug_name;
    swapchain->requested_extent = desc.extent;
    swapchain->preferred_formats.assign(desc.preferred_formats.begin(), desc.preferred_formats.end());
    swapchain->preferred_present_mode = desc.present_mode;

    core::Result<VkSurfaceKHR> surface = create_surface(impl.instance->instance, desc.window);
    if (!surface) {
        swapchain->device = nullptr; // nothing to tear down yet
        return std::move(surface).error();
    }
    swapchain->surface = *surface;
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(impl.physical_device, impl.queue(QueueType::Graphics).family(),
                                         swapchain->surface, &supported);
    if (supported != VK_TRUE) {
        return core::make_error(core::ErrorCode::NotSupported,
                                "swapchain '{}': the queue cannot present to this window", desc.debug_name);
    }

    swapchain->frame_points.resize(desc.frames_in_flight);
    for (uint32_t slot = 0; slot < desc.frames_in_flight; ++slot) {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        const VkSemaphoreCreateInfo semaphore_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (const VkResult result = impl.vk.vkCreateSemaphore(impl.device, &semaphore_info, nullptr, &semaphore);
            result != VK_SUCCESS) {
            return impl.fail(result, "vkCreateSemaphore (acquire)");
        }
        vulkan::set_debug_name(impl.device, VK_OBJECT_TYPE_SEMAPHORE, semaphore,
                               std::format("{} acquire {}", desc.debug_name, slot));
        swapchain->acquire_semaphores.push_back(semaphore);
    }

    if (desc.extent.width != 0 && desc.extent.height != 0) {
        BYTEE_TRY(swapchain->create_swapchain());
    } else {
        swapchain->needs_recreate = true; // created when the window gets a size
    }
    swapchain->tracker_id = impl.live_objects.add("Swapchain", desc.debug_name);
    impl.logger->debug("swapchain '{}' created: {}x{} {}, {} images, {} frames in flight", desc.debug_name,
                       swapchain->extent.width, swapchain->extent.height, to_string(swapchain->format),
                       swapchain->images.size(), desc.frames_in_flight);
    return Swapchain{swapchain.release()};
}

core::Result<SyncPoint> Device::present(Swapchain& swapchain, CommandList&& commands) {
    BYTEE_ASSERT(is_valid(), "Device is empty");
    BYTEE_ASSERT(swapchain.is_valid() && commands.is_valid(), "present needs a swapchain and a command list");
    detail::SwapchainImpl& state = *get_impl(swapchain);
    BYTEE_ASSERT(state.device == impl_, "Swapchain belongs to another device");
    if (state.acquired_image == std::numeric_limits<uint32_t>::max()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "swapchain '{}': present() needs a valid frame from acquire() first", state.debug_name);
    }

    const uint32_t image = std::exchange(state.acquired_image, std::numeric_limits<uint32_t>::max());
    const uint32_t slot = state.frame_slot;
    const VkSemaphoreSubmitInfo wait{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                     .semaphore = state.acquire_semaphores[slot],
                                     .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
    const VkSemaphoreSubmitInfo signal{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                       .semaphore = state.present_semaphores[image],
                                       .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
    core::Result<SyncPoint> submitted =
        impl_->submit_context(take_context(commands), std::span{&wait, 1}, std::span{&signal, 1});
    if (!submitted) {
        return submitted;
    }
    state.frame_points[slot] = *submitted;
    state.frame_slot = (slot + 1) % static_cast<uint32_t>(state.frame_points.size());

    const VkResult result =
        impl_->queue(QueueType::Graphics).present(state.swapchain, image, state.present_semaphores[image]);
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
        state.needs_recreate = true; // handled by the next acquire(); the frame itself was submitted
    } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
        return core::make_error(core::ErrorCode::NotSupported, "swapchain '{}': the surface was lost",
                                state.debug_name);
    } else if (result != VK_SUCCESS) {
        return impl_->fail(result, "vkQueuePresentKHR");
    }
    return submitted;
}

} // namespace bytee::render::rhi
