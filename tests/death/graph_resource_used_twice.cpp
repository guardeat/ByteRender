// Uses one texture twice in a render graph pass (programmer error: one state per resource per pass). CTest checks
// the assert. Needs a GPU.

#include <bytee/render/graph/render_graph.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

namespace core = bytee::core;
namespace render = bytee::render;
namespace rhi = bytee::render::rhi;

namespace {

class ReadAndWritePass {
public:
    void setup(render::PassBuilder& builder) {
        const render::TextureRef texture =
            builder.create_texture("texture", {.extent = {.width = 4, .height = 4}, .format = rhi::Format::RGBA8Unorm});
        builder.write(texture);
        builder.read(texture); // second use in the same pass
    }
    void execute(render::PassContext& /*context*/) {}
};

} // namespace

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink};
    const rhi::InstanceFactory factory{logger};

    rhi::Instance instance = factory.create_instance({}).value();
    rhi::Device device = instance.create_device({}).value();
    render::RenderGraph graph{device};
    ReadAndWritePass pass;
    graph.add_pass("Read and write", pass);
    return 0;
}
