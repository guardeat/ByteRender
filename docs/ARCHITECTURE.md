# ByteRender — Architecture

Status: **agreed design, pre-implementation**. Update this file before changing any decision below.

## 1. Goals
- Generic rendering library reused across multiple projects (games, tools, viewers).
- Cross-platform: Windows, Linux, macOS.
- API-agnostic rendering interface; Vulkan is the first (and currently only) backend.
- Modern explicit-API model (Vulkan 1.3 / D3D12 / Metal style), no OpenGL-style global state.

Non-goals for v1: mobile, old GPUs without descriptor indexing, runtime backend switching.

## 2. Layers

```
 User projects
 ───────────────────────────────────────────────
 byte::render::rg    Render graph         (knows only the RHI)
 ───────────────────────────────────────────────
 byte::render::rhi   API-agnostic RHI     (public headers, pimpl)
 ───────────────────────────────────────────────
 RHI backend         Vulkan               (private; compiled into the RHI library)
 ───────────────────────────────────────────────
 byte::core          Result, asserts, logging, containers, utilities
```

Rule: **nothing above the backend line sees a Vulkan type.** The render graph speaks in
RHI terms (`ResourceState::ShaderRead`), never `VkImageLayout`.

## 3. Repository layout

```
ByteRender/
├─ CMakeLists.txt
├─ cmake/                      helper modules (shader compile, warnings, options)
├─ modules/
│  ├─ core/
│  │  ├─ include/byte/core/         public headers
│  │  └─ src/
│  ├─ rhi/
│  │  ├─ include/byte/render/rhi/   public, API-agnostic headers
│  │  └─ src/
│  │     ├─ common/                 backend-agnostic implementation helpers
│  │     └─ vulkan/                 Vulkan backend (only place Vulkan is included)
│  └─ render_graph/
│     ├─ include/byte/render/rg/
│     └─ src/
├─ shaders/                    engine GLSL shaders
├─ samples/                    small apps (use GLFW); not part of the library
├─ tests/                      unit tests (doctest)
└─ docs/
```

### CMake targets
| Target | Kind | Links |
|---|---|---|
| `byte::core` | static/shared | — |
| `byte::rhi` | static/shared | `core` PUBLIC; backend deps (volk, VMA) **PRIVATE** |
| `byte::render_graph` | static/shared (INTERFACE until step 11) | `rhi` PUBLIC |

Underlying target names are `byte_renderer_<module>`; consumers use the `byte::` aliases.
Each module has its own export macro (`BYTE_RENDERER_CORE_API`, `BYTE_RENDERER_RHI_API`,
`BYTE_RENDERER_RG_API`) so shared builds export/import correctly across module boundaries.

`BYTE_RENDERER_RHI_BACKEND=Vulkan` selects which `src/<backend>/` folder is compiled into `byte::rhi`.
Consumers only ever link `byte::render_graph` and/or `byte::rhi`.

## 4. Object model (compile-time backend via pimpl)

Every RHI type is a **concrete, move-only class** whose only data member is a pointer to a
backend-defined `Impl`. No vtables, no backend types in headers, one indirection.

```cpp
// modules/rhi/include/byte/render/rhi/Texture.h
namespace byte::render::rhi {

class BYTE_RENDERER_RHI_API Texture {
public:
    Texture() = default;                       // empty / invalid
    ~Texture();                                // hands GPU part to the retire queue
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool is_valid() const;
    Extent3D extent() const;
    Format format() const;
    uint32_t bindless_index() const;

private:
    friend class Device;
    struct Impl;                               // defined in src/vulkan/
    Impl* m_impl = nullptr;
};

} // namespace byte::render::rhi
```

### Creation
- Objects are created by their **parent** or by a **non-static factory object** (DI style):
  `Instance` → `Device` → `Texture`, `Buffer`, `Pipeline`, `Swapchain`, `CommandList`, ...
- No static `create()` methods, no singletons, no global registries.
- `Impl` storage is allocated by the device (pools), not by the user.

```cpp
rhi::Instance  instance{rhi::InstanceDesc{ .app_name = "Sample", .enable_validation = true }};
auto device  = instance.create_device(rhi::DeviceDesc{});           // Result<Device>
auto albedo  = device->create_texture(rhi::TextureDesc{ ... });      // Result<Texture>
```

## 5. Lifetime: CPU ownership vs. GPU lifetime

Two separate lifetimes:
- **CPU lifetime** — the user's `Texture` object. One owner. When it dies, the user's reference is gone.
- **GPU lifetime** — the `VkImage`, its memory and its bindless slot. Owned by the device; dies only when
  the GPU has provably finished all work that could reference it.

Mechanism — **timeline retirement**:
1. Each queue owns a timeline semaphore. Every submit signals an increasing 64-bit value.
2. When a resource object is destroyed, its GPU part is pushed to the device's **retire queue**,
   tagged with the queue's *last submitted* timeline value.
3. The device calls `collect()` at frame begin and on submit: everything with
   `tag <= completed_value` is destroyed (memory freed, bindless slot returned).
4. Resources never referenced by a submitted command list are freed immediately.
5. `device.wait_idle()` drains everything (used at shutdown).

Why not per-use refcounting: no atomics while recording, and it stays correct under bindless,
where a shader may read any slot so exact per-command-list usage is unknowable.

Invariants:
- No RHI object may outlive its `Device`. The device tracks live objects and reports leaks (with debug names) in Debug.
- The retire queue is thread-safe (resources may be destroyed from any thread).

## 6. Binding model — bindless only (v1)

- The device owns one **global descriptor heap**: arrays of sampled textures, storage textures,
  storage buffers and samplers.
- Each resource is written into the heap **once at creation** and gets a `uint32_t` index.
- Shaders receive indices and small per-draw data via **push constants**; larger data via buffers referenced by index.
- One global pipeline layout for all pipelines → no layout/set management in the RHI.
- Bindless slots are retired with the same timeline mechanism as memory (§5).
- Debug builds validate indices passed to the API.

Requirement: descriptor indexing (Vulkan 1.2 core; available on desktop GPUs since ~2016, and on
MoltenVK via Metal argument buffers on Apple Silicon).

```glsl
#extension GL_EXT_nonuniform_qualifier : require
layout(set = 0, binding = 0) uniform texture2D g_textures[];
layout(set = 0, binding = 1) uniform sampler   g_samplers[];
layout(push_constant) uniform PushConstants { uint albedo; uint sampler_id; } pc;

vec4 c = texture(sampler2D(g_textures[nonuniformEXT(pc.albedo)], g_samplers[pc.sampler_id]), uv);
```

## 7. Synchronization & submission
- Vulkan 1.3 features: **dynamic rendering** (no `VkRenderPass`/`VkFramebuffer`) and **synchronization2**.
- The RHI exposes barriers as abstract **resource states** (`Undefined`, `ShaderRead`, `ColorAttachment`,
  `DepthWrite`, `CopySrc`, `CopyDst`, `Present`, ...). The backend maps them to stages/access/layouts.
- The render graph computes barriers automatically; raw RHI users issue them manually.
- Frames in flight: 2 (configurable).

## 8. Threading
- `Device` resource creation/destruction is thread-safe.
- `CommandList`s are recorded one per thread, never shared concurrently.
- Submission happens from a single thread per queue.

## 9. Render graph (v1 scope)
- Passes declare reads/writes of graph resources (transient or imported).
- Compile step: cull unused passes, order passes, compute barriers/state transitions.
- Execute step: records into RHI command lists.
- Later: transient memory aliasing, async compute, multi-queue.

## 10. Shaders
- Language: GLSL. Compiled to SPIR-V **at build time** by CMake (`glslangValidator` from the Vulkan SDK).
- Reflection with SPIRV-Reflect where needed (validation of push-constant sizes etc.).
- Future non-Vulkan backends can convert SPIR-V via SPIRV-Cross.

## 11. Conventions
- Depth range [0, 1]. **Reverse-Z by default**: depth cleared to 0, `GREATER` compare, infinite far plane supported (industry standard for depth precision).
- Right-handed world space, Y-up. Clip-space conventions are fixed by the RHI so all backends match.
- Windowing is **not** owned by the engine: it accepts a native window handle
  (`HWND` / X11 / Wayland / `CAMetalLayer`) through an API-agnostic `NativeWindow` struct.

## 12. Error handling
- No exceptions thrown by the engine. Fallible operations return `byte::core::Result<T>`
  (own small `expected`-like type, since C++20 has no `std::expected`).
- `BYTE_RENDERER_ASSERT(cond, msg)` for programmer errors: on in Debug or with
  `BYTE_RENDERER_ASSERTS=ON`, compiled out otherwise.
- Logging goes through a user-replaceable log sink passed in at `Instance` creation (no global logger).

## 13. Dependencies (pinned, via FetchContent)
| Dependency | Used by | Visibility |
|---|---|---|
| Vulkan SDK (headers, validation layers, glslang) | Vulkan backend, shader build | system install |
| volk (Vulkan meta-loader) | Vulkan backend | private |
| Vulkan Memory Allocator | Vulkan backend | private |
| SPIRV-Reflect | Vulkan backend / tools | private |
| GLFW | samples only | — |
| doctest | tests only | — |

## 14. Build options
| Option | Default | Meaning |
|---|---|---|
| `BYTE_RENDERER_RHI_BACKEND` | `Vulkan` | Compile-time backend selection |
| `BYTE_RENDERER_BUILD_SHARED` | `OFF` | Build shared libraries instead of static |
| `BYTE_RENDERER_ASSERTS` | `AUTO` | `BYTE_RENDERER_ASSERT`: `AUTO` = Debug only, `ON`, `OFF` |
| `BYTE_RENDERER_WARNINGS_AS_ERRORS` | `ON` | Warnings are errors in ByteRender targets |
| `BYTE_RENDERER_BUILD_SAMPLES` | `ON` (top-level) | Build sample apps |
| `BYTE_RENDERER_BUILD_TESTS` | `ON` (top-level) | Build unit tests |
