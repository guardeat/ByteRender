// White-box GPU tests of resource lifetime (static builds, label "gpu"): deferred destruction is checked against
// the pools and VMA's live allocation count while the GPU is deliberately blocked.

#include "color.frag.hpp"
#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"
#include "support/vulkan_gpu_gate.hpp"
#include "triangle.vert.hpp"

#include <doctest/doctest.h>

#include <array>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::GpuFixture;

using bytee::tests::GpuGate;
using bytee::tests::live_allocations;

TEST_CASE_FIXTURE(DeviceFixture, "Resources are destroyed immediately when the GPU is idle") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    rhi::BufferHandle handle;
    {
        auto buffer = device.create_buffer({.size = 256, .usage = rhi::BufferUsage::Uniform});
        REQUIRE(buffer.has_value());
        handle = *buffer;
        CHECK(live_allocations(impl) == 1);
    }
    CHECK_FALSE(impl.buffers.contains(handle));
    CHECK(live_allocations(impl) == 0);
    CHECK(impl.live_objects.count() == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Released resources live until the GPU has finished in-flight work") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    GpuGate gate{impl};

    rhi::BufferHandle buffer;
    rhi::TextureHandle texture;
    {
        auto created_buffer = device.create_buffer({.size = 256, .usage = rhi::BufferUsage::Storage});
        auto created_texture = device.create_texture({.extent = {.width = 16, .height = 16},
                                                      .format = rhi::Format::RGBA8Unorm,
                                                      .usage = rhi::TextureUsage::Sampled});
        REQUIRE(created_buffer.has_value());
        REQUIRE(created_texture.has_value());
        buffer = *created_buffer;
        texture = *created_texture;
    } // both released while the GPU is provably busy

    CHECK(impl.buffers.contains(buffer));
    CHECK(impl.textures.contains(texture));
    CHECK(live_allocations(impl) == 2);
    REQUIRE(impl.collect().has_value());
    CHECK(impl.buffers.contains(buffer)); // still in flight

    gate.open();
    REQUIRE(device.wait(gate.blocked()).has_value()); // wait() collects
    CHECK_FALSE(impl.buffers.contains(buffer));
    CHECK_FALSE(impl.textures.contains(texture));
    CHECK(live_allocations(impl) == 0);
    CHECK(impl.live_objects.count() == 0);
}

TEST_CASE_FIXTURE(GpuFixture, "Destroying a device frees pending resources without reporting leaks") {
    auto instance = create_instance();
    REQUIRE(instance.has_value());
    auto device = instance->create_device({.debug_name = "Short-lived"});
    REQUIRE(device.has_value());
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(*device);

    {
        GpuGate gate{impl};
        auto texture = device->create_texture({.extent = {.width = 8, .height = 8},
                                               .format = rhi::Format::RGBA8Unorm,
                                               .usage = rhi::TextureUsage::Sampled,
                                               .debug_name = "pending"});
        REQUIRE(texture.has_value());
        *texture = rhi::Texture{}; // retired, still pending
        CHECK(impl.retired_textures.size() == 1);
        gate.open();
    }
    *device = rhi::Device{}; // must not report 'pending' as a leak (the fixture fails on errors)
}

TEST_CASE_FIXTURE(DeviceFixture, "A released pipeline lives until the GPU has finished in-flight work") {
    rhi::detail::DeviceImpl& impl = *rhi::get_impl(device);
    static constexpr std::array k_formats{rhi::Format::RGBA8Unorm};
    auto pipeline = device.create_graphics_pipeline({.vertex = {.spirv = shaders::triangle_vert},
                                                     .fragment = {.spirv = shaders::color_frag},
                                                     .color_formats = k_formats,
                                                     .debug_name = "deferred pipeline"});
    REQUIRE_MESSAGE(pipeline.has_value(), bytee::tests::error_text(pipeline));
    const rhi::GraphicsPipelineHandle handle = *pipeline;

    GpuGate gate{impl};
    *pipeline = rhi::GraphicsPipeline{}; // released while the GPU is provably busy
    CHECK(impl.graphics_pipelines.contains(handle));
    CHECK(impl.live_objects.count() == 1);

    gate.open();
    REQUIRE(device.wait(gate.blocked()).has_value());
    CHECK_FALSE(impl.graphics_pipelines.contains(handle));
    CHECK(impl.live_objects.count() == 0);
}
