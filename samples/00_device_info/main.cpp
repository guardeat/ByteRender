// Prints the ByteRender version and the compiled-in RHI backend. Grows into a GPU listing in roadmap step 4.

#include <byte/core/log.hpp>
#include <byte/core/version.hpp>
#include <byte/render/rhi/backend.hpp>

int main() {
    byte::core::ConsoleLogSink sink;
    byte::core::Logger logger{sink, byte::core::LogLevel::Info};

    logger.info("ByteRender {} | RHI backend: {}", byte::core::version_string(), byte::render::rhi::backend_name());
    return 0;
}
