// Buffers and textures through the public API (GPU tests). The fixtures fail any test that logs a warning or error.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;
using rhi::BufferUsage;
using rhi::Format;
using rhi::TextureUsage;

static_assert(sizeof(rhi::Buffer) == 16 && sizeof(rhi::Texture) == 16);
static_assert(sizeof(rhi::BufferHandle) == 8 && sizeof(rhi::TextureHandle) == 8);
static_assert(std::is_convertible_v<const rhi::Buffer&, rhi::BufferHandle>, "an owner works as a handle");
static_assert(!std::is_convertible_v<rhi::BufferHandle, rhi::TextureHandle>, "handle kinds do not mix");
static_assert(!std::is_copy_constructible_v<rhi::Texture>, "owners are move-only");

TEST_CASE_FIXTURE(DeviceFixture, "Buffers in every memory location") {
    for (const rhi::MemoryLocation memory :
         {rhi::MemoryLocation::Device, rhi::MemoryLocation::Upload, rhi::MemoryLocation::Readback}) {
        auto buffer = device.create_buffer({.size = 1024,
                                            .usage = BufferUsage::Storage | BufferUsage::TransferDst,
                                            .memory = memory,
                                            .debug_name = "b"});
        REQUIRE_MESSAGE(buffer.has_value(), bytee::tests::error_text(buffer));

        const rhi::BufferInfo& info = device.info(*buffer); // an owner is usable as a handle
        CHECK(info.size == 1024);
        CHECK(info.memory == memory);
        CHECK(info.usage.has(BufferUsage::Storage));
        CHECK(info.device_address != 0);
        CHECK(info.debug_name == "b");
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "CPU writes and reads of host-visible buffers") {
    for (const rhi::MemoryLocation memory : {rhi::MemoryLocation::Upload, rhi::MemoryLocation::Readback}) {
        auto buffer = device.create_buffer({.size = 256, .usage = BufferUsage::TransferSrc, .memory = memory});
        REQUIRE(buffer.has_value());

        std::array<std::byte, 64> pattern{};
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            pattern[i] = static_cast<std::byte>(i * 3 + 1);
        }
        REQUIRE(device.write(*buffer, 100, pattern).has_value());

        std::array<std::byte, 64> back{};
        REQUIRE(device.read(*buffer, 100, back).has_value());
        CHECK(back == pattern);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Invalid buffer descriptions are rejected") {
    auto empty = device.create_buffer({.size = 0, .usage = BufferUsage::Vertex});
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error().code() == core::ErrorCode::InvalidArgument);

    auto no_usage = device.create_buffer({.size = 16});
    REQUIRE_FALSE(no_usage.has_value());
    CHECK(no_usage.error().code() == core::ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(DeviceFixture, "Textures of various kinds") {
    const std::vector<rhi::TextureDesc> descs{
        {.extent = {.width = 64, .height = 32},
         .format = Format::RGBA8Srgb,
         .usage = TextureUsage::Sampled,
         .debug_name = "2d"},
        {.extent = {.width = 256, .height = 256},
         .mip_levels = rhi::full_mip_count({.width = 256, .height = 256}),
         .format = Format::RGBA8Unorm,
         .usage = TextureUsage::Sampled | TextureUsage::TransferDst | TextureUsage::TransferSrc,
         .debug_name = "mipmapped"},
        {.extent = {.width = 16, .height = 16},
         .array_layers = 6,
         .format = Format::RGBA16Float,
         .usage = TextureUsage::Sampled | TextureUsage::ColorAttachment,
         .debug_name = "array"},
        {.dimension = rhi::TextureDimension::Texture3D,
         .extent = {.width = 8, .height = 8, .depth = 8},
         .format = Format::R32Float,
         .usage = TextureUsage::Sampled | TextureUsage::Storage,
         .debug_name = "volume"},
        {.extent = {.width = 128, .height = 128},
         .format = Format::D32Float,
         .usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled,
         .debug_name = "depth"},
        // Regression: transfer-only textures must not get an image view (invalid in Vulkan).
        {.extent = {.width = 8, .height = 8},
         .format = Format::RGBA8Unorm,
         .usage = TextureUsage::TransferSrc | TextureUsage::TransferDst,
         .debug_name = "transfer-only"},
    };
    for (const rhi::TextureDesc& desc : descs) {
        auto texture = device.create_texture(desc);
        REQUIRE_MESSAGE(texture.has_value(), bytee::tests::error_text(texture));
        const rhi::TextureInfo& info = device.info(*texture);
        CHECK(info.dimension == desc.dimension);
        CHECK(info.extent == desc.extent);
        CHECK(info.mip_levels == desc.mip_levels);
        CHECK(info.array_layers == desc.array_layers);
        CHECK(info.format == desc.format);
        CHECK(info.usage == desc.usage);
        CHECK(info.debug_name == desc.debug_name);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Invalid texture descriptions are rejected") {
    const rhi::TextureDesc valid{
        .extent = {.width = 8, .height = 8}, .format = Format::RGBA8Unorm, .usage = TextureUsage::Sampled};
    auto with = [&valid](auto change) {
        rhi::TextureDesc desc = valid;
        change(desc);
        return desc;
    };
    const std::vector<rhi::TextureDesc> invalid{
        with([](rhi::TextureDesc& d) { d.format = Format::Undefined; }),
        with([](rhi::TextureDesc& d) { d.usage = {}; }),
        with([](rhi::TextureDesc& d) { d.extent.width = 0; }),
        with([](rhi::TextureDesc& d) { d.extent.depth = 2; }),                             // 2D with depth
        with([](rhi::TextureDesc& d) { d.mip_levels = 5; }),                               // 8x8 has 4 levels
        with([](rhi::TextureDesc& d) { d.extent.width = 1u << 20; }),                      // beyond device limits
        with([](rhi::TextureDesc& d) { d.usage = TextureUsage::DepthStencilAttachment; }), // color format
        with([](rhi::TextureDesc& d) {
            d.format = Format::D32Float;
            d.usage = TextureUsage::ColorAttachment;
        }),
    };
    for (const rhi::TextureDesc& desc : invalid) {
        auto texture = device.create_texture(desc);
        REQUIRE_FALSE(texture.has_value());
        CHECK(texture.error().code() == core::ErrorCode::InvalidArgument);
    }
}

TEST_CASE_FIXTURE(GpuFixture, "Pool limits are enforced and freed slots are reused") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.limits = {.max_buffers = 2, .max_textures = 1}});
    REQUIRE(device.has_value());

    const rhi::BufferDesc desc{.size = 64, .usage = BufferUsage::Uniform};
    auto first = device->create_buffer(desc);
    auto second = device->create_buffer(desc);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    auto third = device->create_buffer(desc);
    REQUIRE_FALSE(third.has_value());
    CHECK(third.error().code() == core::ErrorCode::LimitExceeded);

    *first = rhi::Buffer{}; // no GPU work in flight: destroyed immediately, slot free again
    auto again = device->create_buffer(desc);
    CHECK_MESSAGE(again.has_value(), bytee::tests::error_text(again));

    auto zero_limits = instance->create_device({.limits = {.max_buffers = 0}});
    REQUIRE_FALSE(zero_limits.has_value());
    CHECK(zero_limits.error().code() == core::ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(DeviceFixture, "Resources move, and churn leaves nothing behind") {
    auto created = device.create_texture(
        {.extent = {.width = 4, .height = 4}, .format = Format::RGBA8Unorm, .usage = TextureUsage::Sampled});
    REQUIRE(created.has_value());
    rhi::Texture texture = std::move(created).value();
    const rhi::TextureHandle handle = texture;
    rhi::Texture moved = std::move(texture);
    CHECK_FALSE(texture.is_valid()); // NOLINT(bugprone-use-after-move): checking the moved-from state
    CHECK(moved.handle() == handle);

    for (int i = 0; i < 500; ++i) {
        auto buffer = device.create_buffer({.size = 4096, .usage = BufferUsage::Storage});
        REQUIRE(buffer.has_value());
    }
    REQUIRE(device.wait_idle().has_value());
}
