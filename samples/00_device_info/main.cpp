// Lists the GPUs Bytee can see, whether each one meets Bytee's requirements, and creates a device on the best one.

#include <bytee/core/log.hpp>
#include <bytee/core/version.hpp>
#include <bytee/render/rhi/backend.hpp>
#include <bytee/render/rhi/instance_factory.hpp>

namespace core = bytee::core;
namespace rhi = bytee::render::rhi;

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Info};

    logger.info("Bytee {} | RHI backend: {}", core::version_string(), rhi::backend_name());

    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance = factory.create_instance({
        .app_name = "00_device_info",
        .app_version = {.major = 0, .minor = 1, .patch = 0},
        .enable_validation = true,
    });
    if (!instance) {
        logger.error("cannot create an instance: {}", instance.error().message());
        return 1;
    }

    for (const rhi::AdapterInfo& adapter : instance->adapters()) {
        logger.info("adapter {}: {} | {} | {} {} | API {}.{}.{} | {} MiB device-local | {}", adapter.index,
                    adapter.name, rhi::to_string(adapter.type), adapter.driver_name, adapter.driver_info,
                    adapter.api_version.major, adapter.api_version.minor, adapter.api_version.patch,
                    adapter.device_local_memory_bytes / (1024 * 1024),
                    adapter.is_supported ? "supported" : "unsupported: " + adapter.unsupported_reason);
    }

    core::Result<rhi::Device> device = instance->create_device({.debug_name = "Main Device"});
    if (!device) {
        logger.error("cannot create a device: {}", device.error().message());
        return 1;
    }
    logger.info("device created on {} (presentation: {})", device->adapter().name,
                device->is_presentation_supported() ? "yes" : "no");
    return 0;
}
