# ByteRender — Project Rules

ByteRender is a generic, reusable, cross-platform rendering engine written in C++20.
It is consumed as a library by multiple independent projects.

Read `docs/ARCHITECTURE.md` for the design and `docs/ROADMAP.md` for the step plan.

## Workflow rules
1. We work **one roadmap step at a time**. Never implement beyond the current step.
2. Every step ends with something that **builds and runs** (or tests pass), then **one commit**.
3. Design decisions that change `docs/ARCHITECTURE.md` are discussed first, then documented, then implemented.
4. If a step reveals a problem with an earlier decision, stop and raise it — don't silently work around it.

## Architecture rules (non-negotiable)
1. **Layering:** `core` ← `rhi` ← `render_graph` ← user code. Lower layers never include higher ones.
2. **No backend types above the backend.** No `Vk*`, `vulkan.h`, `volk.h`, or VMA in any public header
   or in `render_graph`. Backend dependencies are linked `PRIVATE`.
3. **RHI is API-agnostic.** Backend is selected at **compile time** (`BYTE_RENDER_RHI_BACKEND`), one backend per build.
4. **No static classes, no singletons, no global mutable state.** Every engine object is an instance.
5. **Creation goes through a parent object or a (non-static) factory object** — dependency-injection style.
   e.g. `instance.create_device(...)`, `device.create_texture(...)`. No static `create()` methods.
6. **Ownership:** RHI objects are move-only RAII values with exactly one owner. No `shared_ptr` in the public API.
   GPU lifetime is handled by the device's timeline-based retire queue (see ARCHITECTURE §5).
7. **No resource may outlive its `Device`.** Debug builds report leaks by name.
8. **Binding model is bindless only** (v1). There is no descriptor-set API.

## Error handling
- The engine **never throws**. Recoverable/runtime failures return `Result<T>`.
- Programmer errors use `BYTE_RENDER_ASSERT(cond, "msg {}", args...)` (std::format syntax), enabled in Debug
  by default (`BYTE_RENDER_ASSERTS=AUTO|ON|OFF`), compiled out otherwise.
- Create errors with `make_error(ErrorCode::X, "msg {}", args...)` or `Error{...}`.
- No RTTI-dependent code (`dynamic_cast`, `typeid`) in the engine.

## Modern C++ (C++20)
- Strings: `std::string_view` for non-owning strings (parameters, returned literals/names),
  `std::string` when the string is owned/stored. No `const char*` in APIs (except at C-API boundaries).
- Formatting: `std::format` / `std::format_string`. No `printf`-style formatting, no iostreams in the engine.
- Ranges of data: `std::span` instead of pointer + count. Fixed sizes: `std::array`.
- `enum class` always; bit flags via `byte::core::Flags<E>` + `BYTE_RENDER_ENABLE_FLAGS(E)`.
- `[[nodiscard]]` on functions whose result must be checked; `constexpr`/`noexcept` where they apply.
- Designated initializers for descriptor structs: `TextureDesc{ .extent = {...}, .format = ... }`.
- `std::source_location` instead of `__FILE__`/`__LINE__`.
- No owning raw pointers / naked `new`/`delete` outside backend allocators and pimpl internals.
- **Templates are used when they are the right fit** (generic containers/types like `Result<T>`, `Flags<E>`,
  type-safe handles, formatting helpers), always **constrained with concepts**. Avoid template-heavy
  metaprogramming in public APIs and never use templates to leak backend types.
- Interfaces for user-provided extension points (e.g. `LogSink`) may use virtual functions; engine RHI objects do not.

## Code style
- Types / classes / enums: `PascalCase` — `Texture`, `TextureDesc`, `Format::RGBA8Unorm`
- Functions / methods / variables: `snake_case` — `create_texture()`, `bindless_index()`
- Members: `m_` prefix — `m_impl`, `m_extent`
- Constants: `k_` prefix — `k_max_frames_in_flight`
- Macros: `BYTE_RENDER_` prefix, `UPPER_SNAKE_CASE`
- Namespaces: `byte::core` (engine-wide), `byte::render::rhi`, `byte::render::rg` (rendering).
  Engine headers define `WIN32_LEAN_AND_MEAN` before any `<windows.h>` include; consumers who include the full `<windows.h>` (which pulls in the SDK `byte` typedef) must include it in a separate TU or define `WIN32_LEAN_AND_MEAN`. Never `using namespace std;` together with `using namespace byte;`.
- Files: `snake_case.hpp` / `snake_case.cpp` (e.g. `texture.hpp`, `vulkan_device.cpp`), one main class per file.
- Formatting is enforced by `.clang-format`. Warnings are errors in engine targets.

## Platforms & toolchain
- Targets: Windows (MSVC, Clang), Linux (GCC, Clang), macOS (Apple Clang, via MoltenVK).
- Minimum compilers (for C++20 incl. `std::format`): GCC 13, Clang 17, MSVC 19.30 (VS 2022),
  Apple Clang 15 with macOS deployment target 13.3.
- Build: CMake ≥ 3.25 (Unix Makefiles for now), dependencies via `FetchContent` with pinned versions.
  The only manual install is the Vulkan SDK.
- Shaders: GLSL, compiled to SPIR-V at build time.
- Libraries are static by default; `BYTE_RENDER_BUILD_SHARED=ON` builds shared libs
  (each module has its own export macro: `BYTE_RENDER_CORE_API`, `BYTE_RENDER_RHI_API`, `BYTE_RENDER_RG_API`).
