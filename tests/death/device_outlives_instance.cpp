// Destroys an Instance while one of its Devices is alive (programmer error). CTest checks the report and the assert
// (see tests/CMakeLists.txt). Needs a GPU.

#include <bytee/render/rhi/instance_factory.hpp>

#include <optional>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink};
    const rhi::InstanceFactory factory{logger};

    std::optional<rhi::Instance> instance = factory.create_instance({}).value();
    rhi::Device device = instance->create_device({.debug_name = "Leaky Device"}).value();
    instance.reset(); // wrong order: the device is still alive
    return 0;
}
