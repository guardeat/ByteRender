// Submits a command list with an open debug label (programmer error). CTest checks the assert. Needs a GPU.

#include <bytee/render/rhi/instance_factory.hpp>

#include <utility>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink};
    const rhi::InstanceFactory factory{logger};

    rhi::Instance instance = factory.create_instance({}).value();
    rhi::Device device = instance.create_device({}).value();
    rhi::CommandList commands = device.begin_commands().value();
    commands.begin_debug_label("never closed");
    (void)device.submit(std::move(commands));
    return 0;
}
