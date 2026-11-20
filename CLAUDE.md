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
3. **RHI is API-agnostic.** Backend is selected at **compile time** (`BYTE_RENDERER_RHI_BACKEND`), one backend per build.
4. **No static classes, no singletons, no global mutable state.** Every engine object is an instance.
5. **Creation goes through a parent object or a (non-static) factory object** — dependency-injection style.
   e.g. `instance.create_device(...)`, `device.create_texture(...)`. No static `create()` methods.
6. **Ownership:** RHI objects are move-only RAII values with exactly one owner. No `shared_ptr` in the public API.
   GPU lifetime is handled by the device's timeline-based retire queue (see ARCHITECTURE §5).
7. **No resource may outlive its `Device`.** Debug builds report leaks by name.
8. **Binding model is bindless only** (v1). There is no descriptor-set API.

## Error handling
- The engine **never throws**. Recoverable/runtime failures return `Result<T>`.
- Programmer errors use `BYTE_RENDERER_ASSERT(cond, "msg")`, enabled in Debug
  (or via `BYTE_RENDERER_ENABLE_ASSERTS`), compiled out otherwise.
- No RTTI-dependent code (`dynamic_cast`, `typeid`) in the engine.

## Code style
- Types / classes / enums: `PascalCase` — `Texture`, `TextureDesc`, `Format::RGBA8Unorm`
- Functions / methods / variables: `snake_case` — `create_texture()`, `bindless_index()`
- Members: `m_` prefix — `m_impl`, `m_extent`
- Constants: `k_` prefix — `k_max_frames_in_flight`
- Macros: `BYTE_RENDERER_` prefix, `UPPER_SNAKE_CASE`
- Namespaces: `byterender::core`, `byterender::rhi`, `byterender::rg`
  (not `byte` — collides with `std::byte` and the Windows SDK `byte` typedef)
- Files: `PascalCase.h` / `PascalCase.cpp`, one main class per file.
- Formatting is enforced by `.clang-format`. Warnings are errors in engine targets.

## Platforms & toolchain
- Targets: Windows (MSVC, Clang), Linux (GCC, Clang), macOS (Apple Clang, via MoltenVK).
- Build: CMake ≥ 3.24, dependencies via `FetchContent` with pinned versions.
  The only manual install is the Vulkan SDK.
- Shaders: GLSL, compiled to SPIR-V at build time.
- Libraries are static by default; `BYTE_RENDERER_BUILD_SHARED=ON` builds shared libs
  (public symbols use `BYTE_RENDERER_API`).
