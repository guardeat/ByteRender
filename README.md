# Bytee

Generic, high-performance, cross-platform (Windows / Linux / macOS) rendering engine in C++20:
an API-agnostic RHI with a compile-time selected backend (Vulkan) and a render graph on top.

- Rules: [`CLAUDE.md`](CLAUDE.md)
- Design: [`docs/architecture.md`](docs/architecture.md)
- Plan: [`docs/roadmap.md`](docs/roadmap.md)

## Requirements
- CMake ≥ 3.25
- A C++20 compiler with `std::format`: GCC 13, Clang 17, MSVC 2022 or Apple Clang 15 (macOS 13.3+)
- Optional: `clang-format` (23) for the `format` / `format_check` targets

## Setup

### macOS
Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home), or use Homebrew:

```sh
brew install cmake clang-format molten-vk vulkan-loader vulkan-headers vulkan-validationlayers vulkan-tools glslang
./tools/macos/fix_homebrew_validation_layer.sh   # once, and again after upgrading vulkan-validationlayers
vulkaninfo --summary                              # should list your GPU
```

Homebrew's validation-layer manifest names its library without a path, which the loader cannot resolve; the script
rewrites it to the absolute path. Without validation Bytee still runs (it logs a warning and continues).

### Windows / Linux
Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home), or on Linux your distribution's Vulkan loader,
validation layers and glslang packages.

## Build

```sh
cmake --preset debug            # configure (presets: debug, release, shared, asan, tsan)
cmake --build --preset debug    # build
ctest --preset debug            # run tests (ctest --preset debug -LE gpu: skip GPU tests)
./build/debug/bin/00_device_info
./build/debug/bin/01_headless_clear            # renders without a window, writes headless_clear.ppm
./build/debug/bin/02_triangle                  # the RGB triangle, headless, writes triangle.ppm
./build/debug/bin/03_window                    # the triangle in a resizable window (macOS / Windows)
./build/debug/bin/04_imgui                     # debug UI (Dear ImGui) rendered by a render graph pass
./build/debug/bin/05_fps                       # first-person scene: WASD + mouse, E throws physics spheres
```

| Preset | What |
|---|---|
| `debug` | Debug, asserts and error locations on |
| `release` | Release with link-time optimization |
| `shared` | Release, shared libraries |
| `asan` | Debug + AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Debug + ThreadSanitizer |

Formatting:

```sh
cmake --build --preset debug --target format        # rewrite sources
cmake --build --preset debug --target format_check  # fail if anything is not formatted
```

## Using Bytee from another project

```cmake
add_subdirectory(Bytee)            # or FetchContent
target_link_libraries(my_app PRIVATE bytee::render_graph)
```

## License

MIT — see [LICENSE](LICENSE).
