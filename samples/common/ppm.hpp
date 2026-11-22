#pragma once

// Writes RGBA8 pixels as a binary PPM (RGB; alpha dropped): dependency-free and viewable in most image viewers.
// Header-only, so headless samples can use it on every platform.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>

namespace bytee::samples {

[[nodiscard]] inline bool write_ppm(const std::string& path, uint32_t width, uint32_t height,
                                    std::span<const std::byte> rgba) {
    std::ofstream file{path, std::ios::binary};
    file << "P6\n" << width << ' ' << height << "\n255\n";
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        file.write(reinterpret_cast<const char*>(&rgba[i]), 3);
    }
    return static_cast<bool>(file);
}

} // namespace bytee::samples
