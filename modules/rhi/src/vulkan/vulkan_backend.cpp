#include <byte/render/rhi/backend.hpp>

namespace byte::render::rhi {

BackendType backend_type() {
    return BackendType::Vulkan;
}

const char* backend_name() {
    return "Vulkan";
}

} // namespace byte::render::rhi
