#include <bytee/render/rhi/backend.hpp>

namespace bytee::render::rhi {

BackendType backend_type() noexcept {
    return BackendType::Vulkan;
}

std::string_view backend_name() noexcept {
    return "Vulkan";
}

} // namespace bytee::render::rhi
