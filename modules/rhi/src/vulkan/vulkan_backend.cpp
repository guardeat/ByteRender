#include <byte/render/rhi/backend.hpp>

namespace byte::render::rhi {

BackendType backend_type() noexcept {
    return BackendType::Vulkan;
}

std::string_view backend_name() noexcept {
    return "Vulkan";
}

} // namespace byte::render::rhi
