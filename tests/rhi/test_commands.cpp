// Command lists, barriers, copies and uploads through the public API (GPU tests). Every test checks real data that
// travelled through the GPU; the fixture fails on any validation warning or error.

#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using rhi::BufferUsage;
using rhi::ResourceState;
using rhi::TextureUsage;

namespace {

rhi::Buffer make_readback(rhi::Device& device, uint64_t size) {
    return device
        .create_buffer({.size = size,
                        .usage = BufferUsage::CopyDst,
                        .memory = rhi::MemoryLocation::Readback,
                        .debug_name = "readback"})
        .value();
}

template <class Value_>
std::vector<Value_> read_all(rhi::Device& device, rhi::BufferHandle buffer, std::size_t count) {
    std::vector<Value_> values(count);
    REQUIRE(device.read(buffer, 0, std::as_writable_bytes(std::span{values})).has_value());
    return values;
}

std::vector<std::byte> pattern(std::size_t size, uint32_t seed) {
    std::vector<std::byte> bytes(size);
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::byte>((i * 31 + seed) & 0xFFu);
    }
    return bytes;
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "fill_buffer writes through the GPU") {
    rhi::Buffer readback = make_readback(device, 64);

    auto commands = device.begin_commands();
    REQUIRE_MESSAGE(commands.has_value(), bytee::tests::error_text(commands));
    commands->fill_buffer(readback, 0, 64, 0xDEADBEEFu);
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));
    REQUIRE(device.wait(*done).has_value());

    for (const uint32_t value : read_all<uint32_t>(device, readback, 16)) {
        CHECK(value == 0xDEADBEEFu);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Buffer upload and copy with offsets") {
    const std::vector<std::byte> data = pattern(1000, 7);
    rhi::Buffer gpu =
        device.create_buffer({.size = 4096, .usage = BufferUsage::CopyDst | BufferUsage::CopySrc, .debug_name = "gpu"})
            .value();
    rhi::Buffer readback = make_readback(device, 2048);

    REQUIRE(device.upload(gpu, 100, data).has_value()); // later work on the queue may use it without waiting

    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->copy_buffer(gpu, 100, readback, 500, data.size());
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value());

    std::vector<std::byte> back(data.size());
    REQUIRE(device.read(readback, 500, back).has_value());
    CHECK(back == data);
}

TEST_CASE_FIXTURE(DeviceFixture, "Texture uploads and readback of mip levels and array layers") {
    rhi::Texture texture = device
                               .create_texture({.extent = {.width = 32, .height = 16},
                                                .mip_levels = 2,
                                                .array_layers = 2,
                                                .format = rhi::Format::RGBA8Unorm,
                                                .usage = TextureUsage::CopyDst | TextureUsage::CopySrc,
                                                .debug_name = "uploaded"})
                               .value();
    const std::vector<std::byte> mip0_layer0 = pattern(32 * 16 * 4, 1);
    const std::vector<std::byte> mip1_layer1 = pattern(16 * 8 * 4, 2);

    REQUIRE(device.upload(texture, mip0_layer0, {.after = ResourceState::CopySrc}).has_value());
    REQUIRE(device
                .upload(texture, mip1_layer1,
                        {.region = {.mip_level = 1, .base_array_layer = 1}, .after = ResourceState::CopySrc})
                .has_value());

    rhi::Buffer readback = make_readback(device, mip0_layer0.size() + mip1_layer1.size());
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->copy_texture_to_buffer(texture, {}, readback, 0);
    commands->copy_texture_to_buffer(texture, {.mip_level = 1, .base_array_layer = 1}, readback, mip0_layer0.size());
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value());

    std::vector<std::byte> back(mip0_layer0.size() + mip1_layer1.size());
    REQUIRE(device.read(readback, 0, back).has_value());
    CHECK(std::memcmp(back.data(), mip0_layer0.data(), mip0_layer0.size()) == 0);
    CHECK(std::memcmp(back.data() + mip0_layer0.size(), mip1_layer1.data(), mip1_layer1.size()) == 0);
}

TEST_CASE_FIXTURE(DeviceFixture, "Explicit barriers between GPU writes and copies") {
    rhi::Buffer gpu = device.create_buffer({.size = 256, .usage = BufferUsage::CopyDst | BufferUsage::CopySrc}).value();
    rhi::Buffer readback = make_readback(device, 256);

    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->fill_buffer(gpu, 0, 256, 0x01020304u);
    const rhi::BufferBarrier after_fill{
        .buffer = gpu, .before = ResourceState::CopyDst, .after = ResourceState::CopySrc};
    commands->barrier(std::span{&after_fill, 1}, {});
    commands->copy_buffer(gpu, 0, readback, 0, 256);
    auto done = device.submit(std::move(*commands));
    REQUIRE(done.has_value());
    REQUIRE(device.wait(*done).has_value());

    for (const uint32_t value : read_all<uint32_t>(device, readback, 64)) {
        CHECK(value == 0x01020304u);
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Command lists record and submit on several threads at once") {
    constexpr uint32_t k_threads = 4;
    std::vector<rhi::Buffer> targets;
    for (uint32_t i = 0; i < k_threads; ++i) {
        targets.push_back(make_readback(device, 1024));
    }

    std::array<core::Result<rhi::SyncPoint>, k_threads> results{core::Error{}, core::Error{}, core::Error{},
                                                                core::Error{}};
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < k_threads; ++i) {
        threads.emplace_back([&, i] {
            auto commands = device.begin_commands();
            if (!commands) {
                results[i] = std::move(commands).error();
                return;
            }
            for (int repeat = 0; repeat < 16; ++repeat) {
                commands->fill_buffer(targets[i], 0, 1024, 0x1000u + i);
            }
            results[i] = device.submit(std::move(*commands));
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (uint32_t i = 0; i < k_threads; ++i) {
        REQUIRE_MESSAGE(results[i].has_value(), bytee::tests::error_text(results[i]));
        REQUIRE(device.wait(*results[i]).has_value());
        for (const uint32_t value : read_all<uint32_t>(device, targets[i], 256)) {
            CHECK(value == 0x1000u + i);
        }
    }
}

TEST_CASE_FIXTURE(DeviceFixture, "Discarded and moved command lists") {
    {
        auto discarded = device.begin_commands();
        REQUIRE(discarded.has_value());
    } // destroyed without submitting: its commands are dropped

    auto created = device.begin_commands();
    REQUIRE(created.has_value());
    rhi::CommandList list = std::move(created).value();
    rhi::CommandList moved = std::move(list);
    CHECK_FALSE(list.is_valid()); // NOLINT(bugprone-use-after-move): checking the moved-from state
    CHECK(moved.is_valid());

    auto first = device.submit(std::move(moved));
    REQUIRE(first.has_value());
    auto second = device.submit(device.begin_commands().value());
    REQUIRE(second.has_value());
    CHECK(second->value > first->value);
    REQUIRE(device.wait(*second).has_value());
    CHECK(device.is_complete(*first));
}

TEST_CASE_FIXTURE(DeviceFixture, "Upload size mismatches are rejected") {
    rhi::Buffer buffer = device.create_buffer({.size = 64, .usage = BufferUsage::CopyDst}).value();
    const std::vector<std::byte> too_big(65);
    auto overflow = device.upload(buffer, 0, too_big);
    REQUIRE_FALSE(overflow.has_value());
    CHECK(overflow.error().code() == core::ErrorCode::InvalidArgument);

    rhi::Texture texture = device
                               .create_texture({.extent = {.width = 4, .height = 4},
                                                .format = rhi::Format::RGBA8Unorm,
                                                .usage = TextureUsage::CopyDst | TextureUsage::Sampled})
                               .value();
    const std::vector<std::byte> wrong_size(10);
    auto mismatch = device.upload(texture, wrong_size, {});
    REQUIRE_FALSE(mismatch.has_value());
    CHECK(mismatch.error().code() == core::ErrorCode::InvalidArgument);

    const std::vector<std::byte> one_texel(4);
    auto outside = device.upload(texture, one_texel, {.region = {.mip_level = 3}}); // a 4x4 texture has 1 level here
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().code() == core::ErrorCode::InvalidArgument);
}

TEST_CASE_FIXTURE(DeviceFixture, "Debug labels nest around commands") {
    rhi::Buffer buffer =
        device.create_buffer({.size = 64, .usage = rhi::BufferUsage::CopyDst, .debug_name = "labeled"}).value();
    auto commands = device.begin_commands();
    REQUIRE(commands.has_value());
    commands->begin_debug_label("Outer");
    commands->begin_debug_label(std::string(300, 'x')); // longer than the internal buffer: truncated, not an error
    commands->fill_buffer(buffer, 0, 64, 0);
    commands->end_debug_label();
    commands->end_debug_label();
    auto done = device.submit(std::move(*commands));
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));
    REQUIRE(device.wait(*done).has_value());
}
