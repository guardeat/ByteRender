// Render graph building (GPU tests: imports query a real device): passes, accesses, attachments, resources.

#include "support/gpu_fixture.hpp"

#include <bytee/render/graph/render_graph.hpp>

#include <doctest/doctest.h>

#include <format>
#include <string>
#include <vector>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;
using bytee::tests::DeviceFixture;
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
