#include <byte/core/log.hpp>
#include <byte/core/version.hpp>
#include <byte/render/rhi/backend.hpp>

int main() {
    byte::core::ConsoleLogSink sink;
    byte::core::Logger logger{sink, byte::core::LogLevel::Debug};

    logger.info("ByteRender {} | RHI backend: {}", byte::core::version_string(),
                byte::render::rhi::backend_name());
    return 0;
}
