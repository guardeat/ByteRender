// Destroys a Device while a CommandList is still alive (programmer error). CTest checks the report and the assert.
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
    std::optional<rhi::Device> device = instance.create_device({.debug_name = "Busy Device"}).value();
    rhi::CommandList commands = device->begin_commands().value();
    device.reset(); // wrong order: the command list is still alive
    return 0;
}
