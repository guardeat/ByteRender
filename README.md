# ByteRender

Generic, high-performance, cross-platform (Windows / Linux / macOS) rendering engine in C++20:
an API-agnostic RHI with a compile-time selected backend (Vulkan) and a render graph on top.

- Rules: [`CLAUDE.md`](CLAUDE.md)
- Design: [`docs/architecture.md`](docs/architecture.md)
- Plan: [`docs/roadmap.md`](docs/roadmap.md)

## Requirements
- CMake ≥ 3.25
- A C++20 compiler with `std::format`: GCC 13, Clang 17, MSVC 2022 or Apple Clang 15 (macOS 13.3+)
- Optional: `clang-format` (23) for the `format` / `format_check` targets

## Build

```sh
cmake --preset debug            # configure (presets: debug, release, shared, asan, tsan)
cmake --build --preset debug    # build
ctest --preset debug            # run tests
./build/debug/bin/00_device_info
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

## Using ByteRender from another project

```cmake
add_subdirectory(ByteRender)            # or FetchContent
target_link_libraries(my_app PRIVATE byte::render_graph)
```
