// Renders the classic RGB triangle without a window (one render graph pass) and writes it to a PPM image.
//   02_triangle [output.ppm]

#include "ppm.hpp"
#include "triangle.vert.hpp"
#include "vertex_color.frag.hpp"

#include <bytee/core/log.hpp>
#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;

namespace {

constexpr uint32_t k_width = 512;
constexpr uint32_t k_height = 512;
constexpr rhi::Format k_format = rhi::Format::RGBA8Srgb;

// Draws the triangle into its target. Owns its pipeline (created once, at load time).
class TrianglePass {
private:
    rhi::GraphicsPipeline pipeline_;
    render::TextureRef target_;

public:
    explicit TrianglePass(rhi::GraphicsPipeline pipeline) noexcept
        : pipeline_(std::move(pipeline)) {}

    void set_target(render::TextureRef target) noexcept { target_ = target; }

    void setup(render::PassBuilder& builder) {
        builder.color_attachment({.texture = target_, .clear = {0.02f, 0.02f, 0.03f, 1.0f}});
    }
    void execute(render::PassContext& context) {
        context.commands().bind_pipeline(pipeline_);
        context.commands().draw(3);
    }
};

core::Result<TrianglePass> create_triangle_pass(rhi::Device& device) {
    static constexpr std::array k_color_formats{k_format};
    core::Result<rhi::GraphicsPipeline> pipeline = device.create_graphics_pipeline({
        .vertex = {.spirv = shaders::triangle_vert},
        .fragment = {.spirv = shaders::vertex_color_frag},
        .color_formats = k_color_formats,
        .debug_name = "triangle",
    });
    if (!pipeline) {
        return std::move(pipeline).error();
    }
    return TrianglePass{std::move(*pipeline)};
}

core::Result<std::vector<std::byte>> render_image(rhi::Device& device) {
    core::Result<TrianglePass> triangle = create_triangle_pass(device);
    if (!triangle) {
        return std::move(triangle).error();
    }
    core::Result<rhi::Texture> target = device.create_texture({
        .extent = {.width = k_width, .height = k_height},
        .format = k_format,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::CopySrc,
        .debug_name = "triangle target",
    });
    if (!target) {
        return std::move(target).error();
    }

    render::RenderGraph graph{device};
    triangle->set_target(graph.import_texture(
        "target", *target, {.before = rhi::ResourceState::Undefined, .after = rhi::ResourceState::CopySrc}));
    graph.add_pass("Triangle", *triangle);
    BYTEE_TRY(graph.execute());

    std::vector<std::byte> pixels(std::size_t{k_width} * k_height * 4);
    BYTEE_TRY(device.download(*target, pixels, {.before = rhi::ResourceState::CopySrc}));
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
    core::Result<std::vector<std::byte>> pixels = render_image(*device);
    if (!pixels) {
        logger.error("rendering failed: {}", pixels.error().message());
        return 1;
    }
    if (!bytee::samples::write_ppm(output, k_width, k_height, *pixels)) {
        logger.error("cannot write '{}'", output);
        return 1;
    }
    logger.info("rendered a {}x{} triangle on {} -> {}", k_width, k_height, device->adapter().name, output);
    return 0;
}
