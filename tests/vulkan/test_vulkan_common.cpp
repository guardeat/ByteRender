// White-box tests of Vulkan backend helpers. No GPU needed (fake enumeration functions).

#include "vulkan/vulkan_common.hpp"

#include <doctest/doctest.h>

#include <vector>

namespace vulkan = bytee::render::rhi::vulkan;
using bytee::core::ErrorCode;

namespace {

// Behaves like a real VkResult enumeration whose item count grows from 2 to 3 right after the first count query:
// the first data call gets VK_INCOMPLETE (with the items that fit), so enumerate() must query again.
VkResult growing_enumeration(int* calls, uint32_t* count, int* items) {
    const uint32_t available = *calls == 0 ? 2u : 3u;
    ++*calls;
    if (items == nullptr) {
        *count = available;
        return VK_SUCCESS;
    }
    const uint32_t written = *count < available ? *count : available;
    for (uint32_t i = 0; i < written; ++i) {
        items[i] = static_cast<int>(i);
    }
    *count = written;
    return written < available ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult failing_enumeration(uint32_t*, int*) {
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

void void_enumeration(uint32_t* count, int* items) {
    if (items == nullptr) {
        *count = 4;
        return;
    }
    for (uint32_t i = 0; i < *count; ++i) {
        items[i] = 10 + static_cast<int>(i);
    }
}

} // namespace

TEST_CASE("enumerate retries when the count changes between calls") {
    int calls = 0;
    const auto items = vulkan::enumerate<int>(growing_enumeration, &calls);
    CHECK(items == std::vector<int>{0, 1, 2});
    CHECK(calls == 4); // count, incomplete data, count again, complete data
}

TEST_CASE("enumerate returns nothing on failure") {
    CHECK(vulkan::enumerate<int>(failing_enumeration).empty());
}

TEST_CASE("enumerate supports void-returning functions") {
    CHECK(vulkan::enumerate<int>(void_enumeration) == std::vector<int>{10, 11, 12, 13});
}

TEST_CASE("VkResult maps to error codes and names") {
    CHECK(vulkan::to_error_code(VK_ERROR_OUT_OF_DEVICE_MEMORY) == ErrorCode::OutOfMemory);
    CHECK(vulkan::to_error_code(VK_ERROR_INCOMPATIBLE_DRIVER) == ErrorCode::NotSupported);
    CHECK(vulkan::to_error_code(VK_ERROR_DEVICE_LOST) == ErrorCode::DeviceLost);
    CHECK(vulkan::to_error_code(VK_TIMEOUT) == ErrorCode::Timeout);
    CHECK(vulkan::to_string(VK_ERROR_LAYER_NOT_PRESENT) == "VK_ERROR_LAYER_NOT_PRESENT");

    const auto error = vulkan::make_vk_error(VK_ERROR_DEVICE_LOST, "vkQueueSubmit2");
    CHECK(error.code() == ErrorCode::DeviceLost);
    CHECK(error.message() == "vkQueueSubmit2 failed: VK_ERROR_DEVICE_LOST");
#if defined(BYTEE_ENABLE_ERROR_LOCATIONS)
    CHECK(std::string_view{error.location().file_name()} == "tests/vulkan/test_vulkan_common.cpp"); // the caller
#endif
}

#include "vulkan/vulkan_convert.hpp"

TEST_CASE("Resource states map to synchronization2 scopes and layouts") {
    using bytee::render::rhi::ResourceState;
    CHECK(vulkan::to_state_scope(ResourceState::CopyDst).layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CHECK(vulkan::to_state_scope(ResourceState::ShaderRead).layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    CHECK(vulkan::to_state_scope(ResourceState::DepthRead).layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL);
    CHECK(vulkan::to_state_scope(ResourceState::ShaderWrite).layout == VK_IMAGE_LAYOUT_GENERAL);
    CHECK(vulkan::to_state_scope(ResourceState::DepthWrite).layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    CHECK(vulkan::to_state_scope(ResourceState::Undefined).stages == VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    CHECK(vulkan::to_state_scope(ResourceState::Undefined).access == VK_ACCESS_2_NONE);
    CHECK(vulkan::to_state_scope(ResourceState::Present).layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    CHECK((vulkan::to_state_scope(ResourceState::CopySrc).access & VK_ACCESS_2_TRANSFER_READ_BIT) != 0);

    CHECK(vulkan::is_buffer_state(ResourceState::VertexBuffer));
    CHECK_FALSE(vulkan::is_buffer_state(ResourceState::ColorAttachment));
    CHECK(vulkan::is_texture_state(ResourceState::DepthRead));
    CHECK_FALSE(vulkan::is_texture_state(ResourceState::IndexBuffer));
}
