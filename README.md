# ByteRender

Generic, cross-platform (Windows / Linux / macOS) rendering engine in C++20 with an
API-agnostic RHI, a compile-time selected backend (Vulkan) and a render graph.

- Rules: [`CLAUDE.md`](CLAUDE.md)
- Design: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- Plan: [`docs/ROADMAP.md`](docs/ROADMAP.md)

## Setup

### macOS
Either install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home), or use Homebrew:

```sh
brew install cmake molten-vk vulkan-loader vulkan-headers vulkan-validationlayers vulkan-tools glslang
```

Homebrew's validation-layer manifest references its library without a path, so the loader can't load it.
Fix the manifest (run once, and again after `brew upgrade vulkan-validationlayers`):

```sh
./tools/macos/fix_homebrew_validation_layer.sh
```

Check with `vulkaninfo --summary`. If validation can't load, ByteRender logs a warning and continues without it.

### Windows / Linux
Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home) (or your distro's Vulkan loader,
validation layers and glslang packages on Linux).

## Build

Requirements: CMake ≥ 3.25, a C++20 compiler with std::format (GCC 13, Clang 17, MSVC 2022, Apple Clang 15).

```sh
cmake -S . -B build                  # Debug by default
cmake --build build -j
./build/bin/hello
ctest --test-dir build --output-on-failure
```

Shared libraries instead of static:

```sh
cmake -S . -B build-shared -DBYTE_RENDER_BUILD_SHARED=ON
cmake --build build-shared -j
```
