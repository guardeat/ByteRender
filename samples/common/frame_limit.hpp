#pragma once

// `--frames N`: windowed samples exit after N frames (automated runs).

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

namespace bytee::samples {

[[nodiscard]] inline std::optional<uint64_t> frame_limit(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view{argv[i]} == "--frames") {
            uint64_t value = 0;
            const std::string_view text{argv[i + 1]};
            if (std::from_chars(text.data(), text.data() + text.size(), value).ec == std::errc{}) {
                return value;
            }
        }
    }
    return std::nullopt;
}

} // namespace bytee::samples
