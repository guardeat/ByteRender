// Prints the ByteRender version and the compiled-in RHI backend. Grows into a GPU listing in roadmap step 4.

#include <byte/core/version.hpp>
#include <byte/render/rhi/backend.hpp>

#include <cstdio>
#include <format>
#include <string>

int main() {
    const std::string line = std::format("ByteRender {} | RHI backend: {}\n", byte::core::version_string(),
                                         byte::render::rhi::backend_name());
    std::fputs(line.c_str(), stdout);
    return 0;
}
