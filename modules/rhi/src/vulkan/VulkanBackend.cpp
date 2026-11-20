#include <byte/render/rhi/Backend.h>

namespace byte::render::rhi {

BackendType backend_type() {
    return BackendType::Vulkan;
}

const char* backend_name() {
    return "Vulkan";
}

} // namespace byte::render::rhi
