#pragma once

#include <byte/core/log.hpp>
#include <byte/core/result.hpp>
#include <byte/render/rhi/export.hpp>
#include <byte/render/rhi/instance.hpp>

namespace byte::render::rhi {

// Creates Instances. Holds the dependencies injected into every Instance it creates
// (currently the logger). The logger must outlive the created Instance; the factory itself may not.
//
//   core::ConsoleLogSink sink;
//   core::Logger logger{sink};
//   InstanceFactory factory{logger};
//   core::Result<Instance> instance = factory.create_instance({.app_name = "Game", .enable_validation = true});
class BYTE_RENDER_RHI_API InstanceFactory {
public:
    explicit InstanceFactory(core::Logger& logger) noexcept : m_logger(&logger) {}

    [[nodiscard]] core::Result<Instance> create_instance(const InstanceDesc& desc) const;

private:
    core::Logger* m_logger;
};

} // namespace byte::render::rhi
