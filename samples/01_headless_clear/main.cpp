// Renders without a window: clears a texture with a render pass, reads it back and writes it to a PPM image.
//   01_headless_clear [output.ppm]

#include "ppm.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

namespace {

constexpr uint32_t k_width = 256;
constexpr uint32_t k_height = 256;

core::Result<std::vector<std::byte>> render(rhi::Device& device) {
    core::Result<rhi::Texture> target = device.create_texture({
        .extent = {.width = k_width, .height = k_height},
        .format = rhi::Format::RGBA8Srgb,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::CopySrc,
        .debug_name = "headless target",
    });
    if (!target) {
        return std::move(target).error();
    }

    core::Result<rhi::CommandList> commands = device.begin_commands();
    if (!commands) {
        return std::move(commands).error();
    }
    const rhi::TextureBarrier to_attachment{
        .texture = *target, .before = rhi::ResourceState::Undefined, .after = rhi::ResourceState::ColorAttachment};
    commands->barrier({}, std::span{&to_attachment, 1});
    const rhi::ColorAttachment color{.texture = *target, .clear = {0.10f, 0.35f, 0.80f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->end_render_pass();
    BYTEE_TRY(device.submit(std::move(*commands)));

    std::vector<std::byte> pixels(std::size_t{k_width} * k_height * 4);
    BYTEE_TRY(device.download(*target, pixels, {.before = rhi::ResourceState::ColorAttachment}));
    return pixels;
}

} // namespace

int main(int argc, char** argv) {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Info};
    const std::string output = argc > 1 ? argv[1] : "headless_clear.ppm";

    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance =
        factory.create_instance({.app_name = "01_headless_clear", .enable_validation = true});
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }
    core::Result<rhi::Device> device = instance->create_device({.debug_name = "Headless Device"});
    if (!device) {
        logger.error("cannot create a device: {}", device.error().message());
        return 1;
    }

    core::Result<std::vector<std::byte>> pixels = render(*device);
    if (!pixels) {
        logger.error("rendering failed: {}", pixels.error().message());
        return 1;
    }
    if (!bytee::samples::write_ppm(output, k_width, k_height, *pixels)) {
        logger.error("cannot write '{}'", output);
        return 1;
    }
    logger.info("rendered {}x{} on {} without a window -> {}", k_width, k_height, device->adapter().name, output);
    return 0;
}
