#include <byte/core/log.hpp>
#include <byte/core/version.hpp>
#include <byte/render/rhi/backend.hpp>
#include <byte/render/rhi/instance_factory.hpp>

namespace core = byte::core;
namespace rhi = byte::render::rhi;

int main() {
    core::ConsoleLogSink sink;
    core::Logger logger{sink, core::LogLevel::Debug};

    logger.info("ByteRender {} | RHI backend: {}", core::version_string(), rhi::backend_name());

    const rhi::InstanceFactory factory{logger};
    core::Result<rhi::Instance> instance = factory.create_instance({
        .app_name = "hello",
        .app_version = {.major = 0, .minor = 1, .patch = 0},
        .enable_validation = true,
    });
    if (!instance) {
        logger.error("failed to create instance: {}", instance.error().message);
        return 1;
    }

    for (const rhi::AdapterInfo& adapter : instance->adapters()) {
        logger.info("adapter {}: {} ({}, {} {}, API {}.{}.{}, {} MiB device-local){}", adapter.index, adapter.name,
                    rhi::to_string(adapter.type), adapter.driver_name, adapter.driver_info,
                    adapter.api_version.major, adapter.api_version.minor, adapter.api_version.patch,
                    adapter.device_local_memory_bytes / (1024 * 1024),
                    adapter.is_supported ? "" : " - unsupported: " + adapter.unsupported_reason);
    }

    core::Result<rhi::Device> device = instance->create_device({.debug_name = "Main Device"});
    if (!device) {
        logger.error("failed to create device: {}", device.error().message);
        return 1;
    }
    logger.info("using {} (presentation: {})", device->adapter().name, device->supports_presentation());

    if (auto idle = device->wait_idle(); !idle) {
        logger.error("wait_idle failed: {}", idle.error().message);
        return 1;
    }
    return 0;
}
