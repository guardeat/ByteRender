// Renders the classic RGB triangle without a window and writes it to a PPM image.
//   02_triangle [output.ppm]

#include "triangle.frag.hpp"
#include "triangle.vert.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

namespace {

constexpr uint32_t k_width = 512;
constexpr uint32_t k_height = 512;
constexpr rhi::Format k_format = rhi::Format::RGBA8Srgb;

bool write_ppm(const std::string& path, const std::vector<std::byte>& rgba) {
    std::ofstream file{path, std::ios::binary};
    file << "P6\n" << k_width << ' ' << k_height << "\n255\n";
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        file.write(reinterpret_cast<const char*>(&rgba[i]), 3); // drop alpha
    }
    return static_cast<bool>(file);
}

core::Result<std::vector<std::byte>> render(rhi::Device& device) {
    // Pipelines are created once, at load time.
    static constexpr std::array k_color_formats{k_format};
    core::Result<rhi::GraphicsPipeline> pipeline = device.create_graphics_pipeline({
        .vertex = {.spirv = shaders::triangle_vert},
        .fragment = {.spirv = shaders::triangle_frag},
        .color_formats = k_color_formats,
        .debug_name = "triangle",
    });
    if (!pipeline) {
        return std::move(pipeline).error();
    }
    core::Result<rhi::Texture> target = device.create_texture({
        .extent = {.width = k_width, .height = k_height},
        .format = k_format,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::TransferSrc,
        .debug_name = "triangle target",
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
    const rhi::ColorAttachment color{.texture = *target, .clear = {0.02f, 0.02f, 0.03f, 1.0f}};
    commands->begin_render_pass({.colors = std::span{&color, 1}});
    commands->bind_pipeline(*pipeline);
    commands->draw(3);
    commands->end_render_pass();
    if (auto submitted = device.submit(std::move(*commands)); !submitted) {
        return std::move(submitted).error();
    }

    std::vector<std::byte> pixels(std::size_t{k_width} * k_height * 4);
    BYTEE_TRY(device.download(*target, pixels, {.before = rhi::ResourceState::ColorAttachment}));
    return pixels;
}

} // namespace

int main(int argc, char** argv) {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Info};
    const std::string output = argc > 1 ? argv[1] : "triangle.ppm";

    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance =
        factory.create_instance({.app_name = "02_triangle", .enable_validation = true});
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }
    core::Result<rhi::Device> device = instance->create_device({.debug_name = "Triangle Device"});
    if (!device) {
        logger.error("cannot create a device: {}", device.error().message());
        return 1;
    }
    core::Result<std::vector<std::byte>> pixels = render(*device);
    if (!pixels) {
        logger.error("rendering failed: {}", pixels.error().message());
        return 1;
    }
    if (!write_ppm(output, *pixels)) {
        logger.error("cannot write '{}'", output);
        return 1;
    }
    logger.info("rendered a {}x{} triangle on {} -> {}", k_width, k_height, device->adapter().name, output);
    return 0;
}
