#include <byte/core/Version.h>
#include <byte/render/rhi/Backend.h>

#include <cstdio>

int main() {
    std::printf("ByteRender %s | RHI backend: %s\n", byte::core::version_string(),
                byte::render::rhi::backend_name());
    return 0;
}
