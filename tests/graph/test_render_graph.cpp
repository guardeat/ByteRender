// Render graph (GPU tests): building, compiling (culling, validation, barriers) and executing, verified on images.
// The fixture fails on any validation warning or error, so missing or wrong automatic barriers fail the tests.

#include "fullscreen_uv.vert.hpp"
#include "gradient.comp.hpp"
#include "support/error_text.hpp"
#include "support/gpu_fixture.hpp"
#include "support/image.hpp"
#include "textured.frag.hpp"

#include <bytee/render/graph/render_graph.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <vector>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
using bytee::tests::Rgba8;
using bytee::tests::Rgba8Image;
using render::GraphBarrier;
using render::ResourceAccess;
using render::ResourceKind;
using rhi::ResourceState;

namespace {

constexpr render::TextureDesc k_color_desc{.extent = {.width = 64, .height = 32}, .format = rhi::Format::RGBA8Unorm};

// Draws into a new color texture and a new depth texture.
class ScenePass {
private:
    render::TextureRef color_;
    render::TextureRef depth_;

public:
    [[nodiscard]] render::TextureRef color() const noexcept { return color_; }
    [[nodiscard]] render::TextureRef depth() const noexcept { return depth_; }

    void setup(render::PassBuilder& builder) {
        color_ = builder.create_texture("scene color", k_color_desc);
        depth_ =
            builder.create_texture("scene depth", {.extent = k_color_desc.extent, .format = rhi::Format::D32Float});
        builder.color_attachment({.texture = color_, .clear = {.r = 0.1f}});
        builder.depth_attachment({.texture = depth_});
    }
    void execute(render::PassContext& /*context*/) {}
};

// Reads its input and writes a new texture of the same shape (compute).
class BlurPass {
private:
    render::TextureRef input_;
    render::TextureRef output_;

public:
    void set_input(render::TextureRef input) noexcept { input_ = input; }
    [[nodiscard]] render::TextureRef output() const noexcept { return output_; }

    void setup(render::PassBuilder& builder) {
        builder.read(input_);
        output_ = builder.create_texture("blurred", builder.desc(input_));
        builder.write(output_);
    }
    void execute(render::PassContext& /*context*/) {}
};

struct NoExecute {
    void setup(render::PassBuilder& builder);
};

struct WrongSetup {
    void setup(int value);
    void execute(render::PassContext& context);
};

} // namespace

static_assert(render::RenderPass<ScenePass>);
static_assert(render::RenderPass<BlurPass>);
static_assert(!render::RenderPass<NoExecute>);
static_assert(!render::RenderPass<WrongSetup>);
static_assert(!render::RenderPass<int>);
static_assert(sizeof(render::TextureRef) == sizeof(uint32_t));

TEST_CASE_FIXTURE(DeviceFixture, "Passes run setup when added, so outputs feed the next pass") {
    render::RenderGraph graph{device};
    ScenePass scene;
    BlurPass blur;

    graph.add_pass("Scene", scene);
    REQUIRE(scene.color().is_valid()); // known right after add_pass
    blur.set_input(scene.color());
    graph.add_pass("Blur", blur);

    REQUIRE(graph.pass_count() == 2);
    CHECK(graph.pass(0).name == "Scene");
    CHECK(graph.pass(1).name == "Blur");
    CHECK(graph.texture_count() == 3);

    const render::TextureView blurred = graph.texture(blur.output());
    CHECK(blurred.name == "blurred");
    CHECK_FALSE(blurred.imported.has_value());
    CHECK(blurred.desc.extent == k_color_desc.extent); // shape taken from the input
    CHECK(blurred.desc.format == k_color_desc.format);
}

TEST_CASE_FIXTURE(DeviceFixture, "Accesses and attachments are recorded per pass, in declaration order") {
    render::RenderGraph graph{device};
    ScenePass scene;
    BlurPass blur;
    graph.add_pass("Scene", scene);
    blur.set_input(scene.color());
    graph.add_pass("Blur", blur);

    const render::PassView scene_view = graph.pass(0);
    const std::vector<ResourceAccess> scene_accesses{scene_view.accesses.begin(), scene_view.accesses.end()};
    CHECK(scene_accesses == std::vector<ResourceAccess>{
                                {ResourceKind::Texture, scene.color().index(), ResourceState::ColorAttachment, true},
                                {ResourceKind::Texture, scene.depth().index(), ResourceState::DepthWrite, true},
                            });
    REQUIRE(scene_view.colors.size() == 1);
    CHECK(scene_view.colors[0].texture == scene.color());
    CHECK(scene_view.colors[0].clear.r == doctest::Approx(0.1f));
    REQUIRE(scene_view.depth.has_value());
    CHECK(scene_view.depth->texture == scene.depth());

    const render::PassView blur_view = graph.pass(1);
    const std::vector<ResourceAccess> blur_accesses{blur_view.accesses.begin(), blur_view.accesses.end()};
    CHECK(blur_accesses == std::vector<ResourceAccess>{
                               {ResourceKind::Texture, scene.color().index(), ResourceState::ShaderRead, false},
                               {ResourceKind::Texture, blur.output().index(), ResourceState::ShaderWrite, true},
                           });
    CHECK(blur_view.colors.empty());
    CHECK_FALSE(blur_view.depth.has_value());
}

TEST_CASE_FIXTURE(DeviceFixture, "Imported resources take their shape from the device and keep their states") {
    rhi::Texture texture = device
                               .create_texture({.extent = {.width = 16, .height = 8},
                                                .format = rhi::Format::RGBA16Float,
                                                .usage = rhi::TextureUsage::ColorAttachment})
                               .value();
    rhi::Buffer buffer =
        device.create_buffer({.size = 256, .usage = rhi::BufferUsage::Storage, .memory = rhi::MemoryLocation::Readback})
            .value();

    render::RenderGraph graph{device};
    const render::TextureRef target =
        graph.import_texture("target", texture, {.before = ResourceState::Undefined, .after = ResourceState::CopySrc});
    const render::BufferRef results =
        graph.import_buffer("results", buffer, {.before = ResourceState::Undefined, .after = ResourceState::HostRead});

    const render::TextureView target_view = graph.texture(target);
    CHECK(target_view.name == "target");
    REQUIRE(target_view.imported.has_value());
    CHECK(*target_view.imported == texture.handle());
    CHECK(target_view.desc.extent == core::Extent3D{.width = 16, .height = 8});
    CHECK(target_view.desc.format == rhi::Format::RGBA16Float);
    CHECK(target_view.states.after == ResourceState::CopySrc);

    const render::BufferView results_view = graph.buffer(results);
    REQUIRE(results_view.imported.has_value());
    CHECK(*results_view.imported == buffer.handle());
    CHECK(results_view.desc.size == 256);
    CHECK(results_view.states.after == ResourceState::HostRead);
}

TEST_CASE_FIXTURE(DeviceFixture, "Names stay intact as the graph grows, and reset() starts over") {
    render::RenderGraph graph{device};
    struct NamedPass {
        render::TextureRef texture;
        void setup(render::PassBuilder& builder) { texture = builder.create_texture("t", k_color_desc); }
        void execute(render::PassContext& /*context*/) {}
    };
    std::vector<NamedPass> passes(100);
    for (std::size_t frame = 0; frame < 2; ++frame) {
        graph.reset();
        CHECK(graph.pass_count() == 0);
        CHECK(graph.texture_count() == 0);
        for (std::size_t i = 0; i < passes.size(); ++i) {
            graph.add_pass(std::format("pass number {}", i), passes[i]);
        }
        REQUIRE(graph.pass_count() == passes.size());
        CHECK(passes.front().texture.index() == 0); // references start over after reset()
        for (uint32_t i = 0; i < graph.pass_count(); ++i) {
            CHECK(graph.pass(i).name == std::format("pass number {}", i));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Compile and execute

namespace {

constexpr uint32_t k_width = 37;
constexpr uint32_t k_height = 21;

struct GradientPushConstants {
    uint32_t output_index = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct SamplePushConstants {
    uint32_t texture_index = 0;
    uint32_t sampler_index = 0;
    float lod = 0.0f;
    uint32_t use_lod = 0;
};

// Compute: writes a gradient into a new transient texture.
class GradientPass {
private:
    rhi::ComputePipeline pipeline_;
    render::TextureRef output_;
    rhi::TextureHandle last_output_; // the GPU texture used in the last execute (pool reuse checks)

public:
    explicit GradientPass(rhi::Device& device)
        : pipeline_(device.create_compute_pipeline({.shader = {.spirv = shaders::gradient_comp}}).value()) {}

    [[nodiscard]] render::TextureRef output() const noexcept { return output_; }
    [[nodiscard]] rhi::TextureHandle last_output() const noexcept { return last_output_; }

    void setup(render::PassBuilder& builder) {
        output_ = builder.create_texture(
            "gradient", {.extent = {.width = k_width, .height = k_height}, .format = rhi::Format::RGBA8Unorm});
        builder.write(output_, ResourceState::ShaderWrite);
    }
    void execute(render::PassContext& context) {
        last_output_ = context.texture(output_);
        context.commands().bind_pipeline(pipeline_);
        context.commands().push_constants(GradientPushConstants{
            .output_index = rhi::bindless_index(last_output_), .width = k_width, .height = k_height});
        context.commands().dispatch(rhi::dispatch_group_count(k_width, 8), rhi::dispatch_group_count(k_height, 8));
    }
};

// Raster: samples its input into a color target, one texel per pixel.
class CopyPass {
private:
    rhi::GraphicsPipeline pipeline_;
    rhi::Sampler sampler_;
    render::TextureRef input_;
    render::TextureRef target_;

public:
    explicit CopyPass(rhi::Device& device) {
        static constexpr std::array k_formats{rhi::Format::RGBA8Unorm};
        pipeline_ = device
                        .create_graphics_pipeline({.vertex = {.spirv = shaders::fullscreen_uv_vert},
                                                   .fragment = {.spirv = shaders::textured_frag},
                                                   .color_formats = k_formats})
                        .value();
        sampler_ = device
                       .create_sampler({.mag_filter = rhi::Filter::Nearest,
                                        .min_filter = rhi::Filter::Nearest,
                                        .mipmap_mode = rhi::MipmapMode::Nearest,
                                        .address_u = rhi::AddressMode::ClampToEdge,
                                        .address_v = rhi::AddressMode::ClampToEdge})
                       .value();
    }

    void set_input(render::TextureRef input) noexcept { input_ = input; }
    void set_target(render::TextureRef target) noexcept { target_ = target; }

    void setup(render::PassBuilder& builder) {
        builder.read(input_, ResourceState::ShaderRead);
        builder.color_attachment({.texture = target_});
    }
    void execute(render::PassContext& context) {
        context.commands().bind_pipeline(pipeline_);
        context.commands().push_constants(
            SamplePushConstants{.texture_index = rhi::bindless_index(context.texture(input_)),
                                .sampler_index = rhi::bindless_index(sampler_)});
        context.commands().draw(3);
    }
};

// Writes a transient nobody reads: must be culled (and its texture never created).
class UnusedPass {
private:
    render::TextureRef output_;

public:
    bool executed = false;

    void setup(render::PassBuilder& builder) {
        output_ = builder.create_texture("unused", k_color_desc);
        builder.color_attachment({.texture = output_});
    }
    void execute(render::PassContext& /*context*/) { executed = true; }
};

Rgba8Image expected_gradient() {
    Rgba8Image expected{.width = k_width, .height = k_height};
    for (uint32_t y = 0; y < k_height; ++y) {
        for (uint32_t x = 0; x < k_width; ++x) {
            expected.pixels.push_back(Rgba8{static_cast<uint8_t>(x * 4), static_cast<uint8_t>(y * 4), 128, 255});
        }
    }
    return expected;
}

rhi::Texture make_target(rhi::Device& device) {
    return device
        .create_texture(
            {.extent = {.width = k_width, .height = k_height},
             .format = rhi::Format::RGBA8Unorm,
             .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst,
             .debug_name = "graph target"})
        .value();
}

// Gradient (compute) -> copy (draw) into `target`, plus an unused pass in between.
void build_frame(render::RenderGraph& graph, GradientPass& gradient, UnusedPass& unused, CopyPass& copy,
                 rhi::TextureHandle target) {
    graph.reset();
    graph.add_pass("Gradient", gradient);
    graph.add_pass("Unused", unused);
    copy.set_input(gradient.output());
    copy.set_target(
        graph.import_texture("target", target, {.before = ResourceState::Undefined, .after = ResourceState::CopySrc}));
    graph.add_pass("Copy", copy);
}

} // namespace

TEST_CASE_FIXTURE(DeviceFixture, "Graph execution: compute feeds a draw, all barriers automatic") {
    rhi::Texture target = make_target(device);
    GradientPass gradient{device};
    UnusedPass unused;
    CopyPass copy{device};
    render::RenderGraph graph{device};
    build_frame(graph, gradient, unused, copy, target);

    auto done = graph.execute();
    REQUIRE_MESSAGE(done.has_value(), bytee::tests::error_text(done));
    CHECK_FALSE(unused.executed);
    const Rgba8Image image = bytee::tests::read_rgba8(device, target, ResourceState::CopySrc);
    bytee::tests::check_image(image, expected_gradient(), 0, "graph_compute_then_draw");
}

TEST_CASE_FIXTURE(DeviceFixture, "Compile: culling, transient usages and barriers") {
    rhi::Texture target = make_target(device);
    GradientPass gradient{device};
    UnusedPass unused;
    CopyPass copy{device};
    render::RenderGraph graph{device};
    build_frame(graph, gradient, unused, copy, target);
    const auto result = graph.compile();
    REQUIRE_MESSAGE(result.has_value(), bytee::tests::error_text(result));

    CHECK_FALSE(graph.pass(0).culled);
    CHECK(graph.pass(1).culled);
    CHECK_FALSE(graph.pass(2).culled);

    const render::TextureView gradient_view = graph.texture(gradient.output());
    CHECK(gradient_view.used);
    CHECK(gradient_view.usage == (rhi::TextureUsage::Storage | rhi::TextureUsage::Sampled));
    CHECK_FALSE(graph.texture(render::TextureRef{1}).used); // the unused pass's texture is never created

    const uint32_t gradient_index = gradient.output().index();
    const uint32_t target_index = 2;
    const auto barriers = [&](uint32_t pass) {
        const render::PassView view = graph.pass(pass);
        return std::vector<GraphBarrier>{view.barriers.begin(), view.barriers.end()};
    };
    CHECK(barriers(0) ==
          std::vector<GraphBarrier>{
              {ResourceKind::Texture, gradient_index, ResourceState::Undefined, ResourceState::ShaderWrite},
          });
    CHECK(barriers(1).empty());
    CHECK(barriers(2) ==
          std::vector<GraphBarrier>{
              {ResourceKind::Texture, gradient_index, ResourceState::ShaderWrite, ResourceState::ShaderRead},
              {ResourceKind::Texture, target_index, ResourceState::Undefined, ResourceState::ColorAttachment},
          });
    const std::vector<GraphBarrier> final{graph.final_barriers().begin(), graph.final_barriers().end()};
    CHECK(final == std::vector<GraphBarrier>{
                       {ResourceKind::Texture, target_index, ResourceState::ColorAttachment, ResourceState::CopySrc},
                   });
}

TEST_CASE_FIXTURE(DeviceFixture, "Reads of the same state need no barrier; imported resources always reach 'after'") {
    rhi::Texture texture = make_target(device);
    render::RenderGraph graph{device};
    struct Reader {
        render::TextureRef input;
        render::TextureRef output;
        void setup(render::PassBuilder& builder) {
            builder.read(input, ResourceState::CopySrc);
            output = builder.create_texture("copy", k_color_desc);
            builder.write(output, ResourceState::CopyDst);
        }
        void execute(render::PassContext& /*context*/) {}
    };
    struct Sink { // keeps the readers alive by consuming their outputs into an imported texture
        std::vector<render::TextureRef> inputs;
        render::TextureRef target;
        void setup(render::PassBuilder& builder) {
            for (const render::TextureRef input : inputs) {
                builder.read(input, ResourceState::CopySrc);
            }
            builder.write(target, ResourceState::CopyDst);
        }
        void execute(render::PassContext& /*context*/) {}
    };
    const render::TextureRef source =
        graph.import_texture("source", texture, {.before = ResourceState::CopySrc, .after = ResourceState::CopySrc});
    rhi::Texture other = make_target(device);
    const render::TextureRef untouched = graph.import_texture(
        "untouched", other, {.before = ResourceState::Undefined, .after = ResourceState::ShaderRead});
    Reader first{.input = source};
    Reader second{.input = source};
    graph.add_pass("First", first);
    graph.add_pass("Second", second);
    rhi::Texture sink_texture = make_target(device);
    Sink sink{.inputs = {first.output, second.output},
              .target = graph.import_texture("sink", sink_texture,
                                             {.before = ResourceState::Undefined, .after = ResourceState::CopyDst})};
    graph.add_pass("Sink", sink);
    REQUIRE(graph.compile().has_value());

    // The first read waits for work before the graph; the second read of the same state needs nothing.
    REQUIRE(graph.pass(0).barriers.size() == 2);
    CHECK(graph.pass(0).barriers[0] ==
          GraphBarrier{ResourceKind::Texture, source.index(), ResourceState::CopySrc, ResourceState::CopySrc});
    REQUIRE(graph.pass(1).barriers.size() == 1); // only its own output
    CHECK(graph.pass(1).barriers[0].resource == second.output.index());

    const auto& final = graph.final_barriers();
    CHECK(std::ranges::find(final, GraphBarrier{ResourceKind::Texture, untouched.index(), ResourceState::Undefined,
                                                ResourceState::ShaderRead}) != final.end());
    CHECK(std::ranges::none_of(final, [&](const GraphBarrier& barrier) { return barrier.resource == source.index(); }));
}

TEST_CASE_FIXTURE(DeviceFixture, "Compile rejects reads before writes and transients in host-only states") {
    rhi::Texture target = make_target(device);
    rhi::Buffer results =
        device.create_buffer({.size = 64, .usage = rhi::BufferUsage::Storage, .memory = rhi::MemoryLocation::Readback})
            .value();

    struct ReadsUnwritten {
        render::TextureRef target;
        void setup(render::PassBuilder& builder) {
            builder.read(builder.create_texture("never written", k_color_desc));
            builder.color_attachment({.texture = target});
        }
        void execute(render::PassContext& /*context*/) {}
    };
    struct HostReadTransient {
        render::BufferRef results;
        void setup(render::PassBuilder& builder) {
            builder.write(builder.create_buffer("scratch", {.size = 64}), ResourceState::HostRead);
            builder.write(results);
        }
        void execute(render::PassContext& /*context*/) {}
    };

    render::RenderGraph graph{device};
    ReadsUnwritten reader{.target = graph.import_texture(
                              "target", target, {.before = ResourceState::Undefined, .after = ResourceState::CopySrc})};
    graph.add_pass("Reader", reader);
    auto compiled = graph.compile();
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == core::ErrorCode::InvalidArgument);
    CHECK(compiled.error().message() == "render graph: pass 'Reader' reads 'never written' before any pass writes it");

    graph.reset();
    HostReadTransient host{
        .results = graph.import_buffer("results", results,
                                       {.before = ResourceState::Undefined, .after = ResourceState::HostRead})};
    graph.add_pass("Host read", host);
    compiled = graph.compile();
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().message().find("transient buffer 'scratch' in state HostRead") != std::string::npos);
}

TEST_CASE_FIXTURE(DeviceFixture, "Transients are reused across frames and released when unused") {
    rhi::Texture target = make_target(device);
    GradientPass gradient{device};
    UnusedPass unused;
    CopyPass copy{device};
    render::RenderGraph graph{device};

    build_frame(graph, gradient, unused, copy, target);
    REQUIRE(graph.execute().has_value());
    const rhi::TextureHandle first = gradient.last_output();
    CHECK(graph.pooled_texture_count() == 1);

    build_frame(graph, gradient, unused, copy, target);
    REQUIRE(graph.execute().has_value());
    CHECK(gradient.last_output() == first); // same GPU texture, no new allocation
    CHECK(graph.pooled_texture_count() == 1);

    for (uint64_t frame = 0; frame <= render::RenderGraph::k_transient_keep_frames; ++frame) {
        graph.reset(); // frames without the gradient pass
    }
    CHECK(graph.pooled_texture_count() == 0);
    REQUIRE(device.wait_idle().has_value());
}

// ---------------------------------------------------------------------------------------------------------------------
// Tooling: Graphviz dump, validation of attachments and imported usages

TEST_CASE_FIXTURE(DeviceFixture, "The Graphviz dump shows passes, resources, states and culling") {
    rhi::Texture target = make_target(device);
    GradientPass gradient{device};
    UnusedPass unused;
    CopyPass copy{device};
    render::RenderGraph graph{device};
    build_frame(graph, gradient, unused, copy, target);
    REQUIRE(graph.compile().has_value());

    const std::string dot = graph.to_graphviz();
    CHECK(dot.starts_with("digraph RenderGraph {"));
    CHECK(dot.ends_with("}\n"));
    CHECK(dot.find(R"dot(pass0 [label="Gradient", shape=box, style="filled")dot") != std::string::npos);
    CHECK(dot.find(R"dot(pass1 [label="Unused (culled)", shape=box, style="filled,dashed")dot") != std::string::npos);
    CHECK(dot.find(R"dot(texture0 [label="gradient\n37x21 RGBA8Unorm", shape=ellipse])dot") != std::string::npos);
    CHECK(dot.find(R"dot(texture2 [label="target\n37x21 RGBA8Unorm", shape=ellipse, style=bold])dot") !=
          std::string::npos);
    CHECK(dot.find(R"dot(pass0 -> texture0 [label="ShaderWrite"])dot") != std::string::npos);
    CHECK(dot.find(R"dot(texture0 -> pass2 [label="ShaderRead"])dot") != std::string::npos);
    CHECK(dot.find(R"dot(pass2 -> texture2 [label="ColorAttachment"])dot") != std::string::npos);
}

TEST_CASE_FIXTURE(DeviceFixture, "Graphviz names are escaped") {
    render::RenderGraph graph{device};
    struct Quoted {
        void setup(render::PassBuilder& builder) {
            (void)builder.create_buffer(R"dot(a "quoted" \ name)dot", {.size = 4});
        }
        void execute(render::PassContext& /*context*/) {}
    };
    Quoted quoted;
    graph.add_pass(R"(say "hi")", quoted);
    const std::string dot = graph.to_graphviz();
    CHECK(dot.find(R"dot(label="say \"hi\"")dot") != std::string::npos);
    CHECK(dot.find(R"dot(label="a \"quoted\" \\ name\n4 bytes")dot") != std::string::npos);
}

TEST_CASE_FIXTURE(DeviceFixture,
                  "Compile rejects attachments of different sizes and imports without the needed usage") {
    struct TwoTargets {
        render::TextureRef first;
        render::TextureRef second;
        void setup(render::PassBuilder& builder) {
            builder.color_attachment({.texture = first});
            builder.color_attachment({.texture = second});
        }
        void execute(render::PassContext& /*context*/) {}
    };
    rhi::Texture big = make_target(device);
    rhi::Texture small = device
                             .create_texture({.extent = {.width = 8, .height = 8},
                                              .format = rhi::Format::RGBA8Unorm,
                                              .usage = rhi::TextureUsage::ColorAttachment})
                             .value();
    rhi::Texture sampled_only = device
                                    .create_texture({.extent = {.width = 8, .height = 8},
                                                     .format = rhi::Format::RGBA8Unorm,
                                                     .usage = rhi::TextureUsage::Sampled})
                                    .value();
    const render::ImportedStates states{.before = ResourceState::Undefined, .after = ResourceState::ColorAttachment};

    render::RenderGraph graph{device};
    TwoTargets mismatched{.first = graph.import_texture("big", big, states),
                          .second = graph.import_texture("small", small, states)};
    graph.add_pass("Two targets", mismatched);
    auto compiled = graph.compile();
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().code() == core::ErrorCode::InvalidArgument);
    CHECK(compiled.error().message() ==
          "render graph: pass 'Two targets' has attachments of different sizes ('big' 37x21, 'small' 8x8)");

    graph.reset();
    TwoTargets wrong_usage{.first = graph.import_texture("small", small, states),
                           .second = graph.import_texture("sampled only", sampled_only, states)};
    graph.add_pass("Wrong usage", wrong_usage);
    compiled = graph.compile();
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error().message() == "render graph: pass 'Wrong usage' uses 'sampled only' as ColorAttachment, but "
                                        "it was created without the usage flags that state needs");
}
