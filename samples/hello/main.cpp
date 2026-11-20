#include <byte/core/version.hpp>
#include <byte/render/rhi/backend.hpp>

#include <cstdio>

int main() {
    std::printf("ByteRender %s | RHI backend: %s\n", byte::core::version_string(),
                byte::render::rhi::backend_name());
    return 0;
}
