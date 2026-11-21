#pragma once

#include <bytee/core/log.hpp>
#include <bytee/core/result.hpp>
#include <bytee/render/rhi/export.hpp>
#include <bytee/render/rhi/instance.hpp>

namespace bytee::render::rhi {

// Creates Instances and injects their dependencies (currently the logger). The logger must outlive every Instance
// created from this factory; the factory itself may be destroyed right after use.
//
//   core::ConsoleLogSink sink;
//   core::Logger logger{sink};
//   const InstanceFactory factory{logger};
//   core::Result<Instance> instance = factory.create_instance({.app_name = "Game", .enable_validation = true});
class BYTEE_RHI_API InstanceFactory {
private:
    core::Logger* logger_;

public:
    explicit InstanceFactory(core::Logger& logger) noexcept
        : logger_(&logger) {}

    // Fails with ErrorCode::NotSupported if no Vulkan 1.3 loader is available or another Instance is alive.
    [[nodiscard]] core::Result<Instance> create_instance(const InstanceDesc& desc) const;
};

} // namespace bytee::render::rhi
