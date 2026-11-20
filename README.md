# ByteRender

Generic, cross-platform (Windows / Linux / macOS) rendering engine in C++20 with an
API-agnostic RHI, a compile-time selected backend (Vulkan) and a render graph.

- Rules: [`CLAUDE.md`](CLAUDE.md)
- Design: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- Plan: [`docs/ROADMAP.md`](docs/ROADMAP.md)

## Build

Requirements: CMake ≥ 3.24, a C++20 compiler.

```sh
cmake -S . -B build                  # Debug by default
cmake --build build -j
./build/bin/hello
```

Shared libraries instead of static:

```sh
cmake -S . -B build-shared -DBYTE_RENDERER_BUILD_SHARED=ON
cmake --build build-shared -j
```
