// Prints the Bytee version and the compiled-in RHI backend. Grows into a GPU listing in roadmap step 4.

#include <bytee/core/log.hpp>
#include <bytee/core/version.hpp>
#include <bytee/render/rhi/backend.hpp>

int main() {
    bytee::core::ConsoleLogSink sink;
    bytee::core::Logger logger{sink, bytee::core::LogLevel::Info};

    logger.info("Bytee {} | RHI backend: {}", bytee::core::version_string(), bytee::render::rhi::backend_name());
    return 0;
}
