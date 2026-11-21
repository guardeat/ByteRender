// Destroys a Device while one of its textures is alive (programmer error). CTest checks the report and the assert.
// Needs a GPU.

#include <bytee/render/rhi/instance_factory.hpp>

#include <optional>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink};
    const rhi::InstanceFactory factory{logger};

    rhi::Instance instance = factory.create_instance({}).value();
    std::optional<rhi::Device> device = instance.create_device({.debug_name = "Leaky Device"}).value();
    rhi::Texture texture = device
                               ->create_texture({.extent = {.width = 4, .height = 4},
                                                 .format = rhi::Format::RGBA8Unorm,
                                                 .usage = rhi::TextureUsage::Sampled,
                                                 .debug_name = "leaked"})
                               .value();
    device.reset(); // wrong order: the texture is still alive
    return 0;
}
