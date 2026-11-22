# ByteRender — Project Rules

ByteRender is a generic, reusable, cross-platform rendering engine in C++20, consumed as a library by
multiple projects. **Performance is a primary goal.**

- Design: `docs/architecture.md`
- Plan: `docs/roadmap.md`

## Workflow
1. One roadmap step at a time. Never implement beyond the current step.
2. Plan the step before coding it; discuss design changes first, update `docs/architecture.md`, then implement.
3. Every step ends with a from-scratch build of **every preset** (`debug`, `release`, `shared`, `asan`, `tsan`),
   tests passing, samples run, `format_check` clean — then **one commit**. Judge every stage by its **exit code**,
   not only by searching logs for warnings (a failing custom command may print no "error:" line).
4. If a step reveals a problem with an earlier decision, stop and raise it — never silently work around it.

## Zero-noise policy (100% clean)
- Configure, build, link and tests produce **zero warnings** (compiler, linker, CMake, third-party build scripts).
- Running samples/tests produces **no warnings, errors or third-party chatter** (loader, validation layer, driver).
  Only intentional engine/sample log lines appear.
- Fix root causes. Suppressing a message is only acceptable when it is provably harmless, and is documented where done.
- GPU tests capture all log output and fail on any warning or error.
- In doctest `*_MESSAGE` macros use `bytee::tests::error_text(result)`, never `result.error().message()` (doctest may
  evaluate messages for passing checks; `error()` on a value asserts). Run suites with `-s` when adding test helpers.
- Code touching concurrency or object lifetimes is verified under ASan+UBSan and TSan before committing.

## Architecture rules (non-negotiable)
1. Layering: `core` ← `render::rhi` ← `render::graph` ← user code. Lower layers never include higher ones.
2. **No backend types above the backend.** No `Vk*`, `<vulkan/vulkan.h>`, `volk.h` or VMA in any public header
   or in the render graph. Backend dependencies are linked `PRIVATE`.
3. The RHI is API-agnostic; the backend is selected at **compile time** (`BYTEE_RHI_BACKEND`). No virtual
   dispatch in the RHI.
4. **No static classes, no singletons, no global mutable state.** Every engine object is an instance.
   Sole exception: process-wide Vulkan loader state owned by volk, hidden in the backend (architecture §10).
5. Creation goes through a **parent object or a non-static factory object** (dependency injection).
   No static `create()` functions.
6. GPU resources are `core::Resource<Tag_, Deleter_>` (move-only RAII, single owner); commands take
   `core::Handle<Tag_>` (8-byte non-owning views). No `shared_ptr` in the API.
7. GPU objects are never destroyed while the GPU may use them: resource deleters retire to the device's
   timeline-based retire lists (architecture §6).
8. No object may outlive its parent (`Device` → resources, `Instance` → `Device`). Leaks are reported by name.
9. Binding model: bindless only.
10. Opaque backend types nested in exported classes are declared `struct BYTEE_HIDDEN Impl;` so shared libraries
    export only the public API (verify with `nm` on the `shared` preset).

## Performance rules
- No heap allocation, exceptions, RTTI, virtual calls or locks on hot paths (command recording, per-draw work).
- Data lives in dense pools; hot paths pass handles, not objects or pointers to objects.
- Logging formats only when the level is enabled; asserts compile out in Release.
- Release builds use LTO. Measure before and after optimizations that complicate code.

## Error handling
- The engine never throws. Fallible operations return `core::Result<T>`; create errors with
  `core::make_error(ErrorCode::X, "message {}", args...)`.
- Programmer errors: `BYTEE_ASSERT(condition, "message {}", args...)` — on in Debug, compiled out otherwise.
- No `dynamic_cast` / `typeid` in the engine.

## Code style
- Types, classes, enums, concepts: `PascalCase` — `Texture`, `TextureDesc`, `Format::RGBA8Unorm`
- Functions, variables: `snake_case` — `create_texture()`, `bindless_index()`
- **Template parameters: `PascalCase_` (trailing underscore)** — `template <class Tag_, class Deleter_>`,
  `template <size_t QueueCount_>`
- Private members: trailing underscore — `impl_`, `entries_`. Public fields of plain structs: plain — `desc.extent`
- Constants: `k_` prefix — `k_max_frames_in_flight`
- Macros: `BYTEE_` prefix, `UPPER_SNAKE_CASE`
- Namespaces (all lowercase): `bytee::core`, `bytee::render::rhi`, `bytee::render::graph`. Macros and code inside
  namespace `std` (e.g. `std::hash` specializations) spell names fully qualified: `::bytee::...`.
- Files: `snake_case.hpp` / `snake_case.cpp`, one main type per file. CMake modules: `snake_case.cmake`.
- Formatting enforced by `.clang-format`. Warnings are errors in engine targets.

### Class / struct layout
In this order, omitting empty sections:
```cpp
class Example {
public:   // 1. public types: nested types and `using` aliases used outside the class
private:  // 2. member variables, plus private nested types used only internally
public:   // 3. public functions: constructors, destructor, operators, methods
private:  // 4. private functions, private constructors, friend declarations
};
```
Plain-data structs list their fields first, then any functions.

## Modern C++ (C++20)
- `std::string_view` for non-owning strings, `std::string` when owned. No `const char*` in APIs.
- `std::format` for formatting; `std::span` instead of pointer + count; `std::array` for fixed sizes.
- `enum class` always; bit flags with `core::Flags<Enum_>`.
- `[[nodiscard]]`, `constexpr`, `noexcept` where they apply. Designated initializers for descriptor structs.
- `std::source_location` instead of `__FILE__` / `__LINE__`.
- No owning raw pointers or naked `new`/`delete` outside pools, allocators and backend root objects.
- Templates when they are the right fit, **always constrained with concepts**. No template metaprogramming
  that hurts readability or compile times without a measured benefit.

## Platforms & toolchain
- Windows (MSVC, Clang), Linux (GCC, Clang), macOS (Apple Clang via MoltenVK).
- Minimum: CMake 3.25; GCC 13, Clang 17, MSVC 19.30 (VS 2022), Apple Clang 15, macOS 13.3.
- Dependencies via `FetchContent`, pinned by version and SHA-256. Only manual install: the Vulkan SDK
  (or Homebrew Vulkan packages on macOS).
- Shaders: GLSL, compiled to SPIR-V at build time.
- Static libraries by default; `BYTEE_BUILD_SHARED=ON` builds shared libraries.
