# ByteRender

Generic, cross-platform (Windows / Linux / macOS) rendering engine in C++20 with an
API-agnostic RHI, a compile-time selected backend (Vulkan) and a render graph.

- Rules: [`CLAUDE.md`](CLAUDE.md)
- Design: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- Plan: [`docs/ROADMAP.md`](docs/ROADMAP.md)

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
