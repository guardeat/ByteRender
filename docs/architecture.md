# ByteRender — Architecture

Status: **agreed design, pre-implementation.** Change this document before changing a decision below.

## 1. Goals
- Generic rendering library reused across multiple projects (games, tools, viewers).
- **Performance first**: data-oriented resource storage, no virtual dispatch, no hot-path allocations.
- Cross-platform: Windows, Linux, macOS. API-agnostic RHI; Vulkan is the first backend.
- Modern explicit-API model (Vulkan 1.3 / D3D12 / Metal style).
- **Headless rendering** is first-class: rendering never requires a window.

Non-goals (v1): mobile, GPUs without descriptor indexing, runtime backend switching.

## 2. Layers

```
 User projects
 ──────────────────────────────────────────────────────────────────────────────
 byte::render::graph   passes, transient resources, automatic barriers       (knows only the RHI)
 ──────────────────────────────────────────────────────────────────────────────
 byte::render::rhi     Instance, Device, CommandList, Swapchain   (objects)
                       Texture, Buffer, ... = core::Resource<Tag_, Deleter_>   (RAII owners)
                       TextureHandle, ...   = core::Handle<Tag_>               (8-byte views)
 ──────────────────────────────────────────────────────────────────────────────
 Vulkan backend        pools, volk device tables, VMA, retire lists, bindless heap   (private)
 ──────────────────────────────────────────────────────────────────────────────
 byte::core            Result, assert, Logger, Flags, Handle, Resource, Pool          (no GPU code)
```

Rule: nothing above the backend line sees a Vulkan type.

## 3. Repository layout

```
ByteRender/
├── CMakeLists.txt  CMakePresets.json  CLAUDE.md  README.md
├── .clang-format  .clang-tidy  .editorconfig  .gitignore
├── cmake/
│   ├── byte_render_options.cmake        BYTE_RENDER_* options
│   ├── byte_render_compiler.cmake       warnings, sanitizers, LTO
│   ├── byte_render_dependencies.cmake   pinned FetchContent dependencies
│   └── byte_render_shaders.cmake        GLSL → SPIR-V
├── docs/            architecture.md  roadmap.md
├── modules/
│   ├── core/
│   │   ├── include/byte/core/           public headers
│   │   └── src/
│   ├── rhi/
│   │   ├── include/byte/render/rhi/     public, API-agnostic headers
│   │   └── src/
│   │       ├── common/                  backend-agnostic internals
│   │       └── vulkan/                  the only place that includes Vulkan / volk / VMA
│   └── render_graph/
│       ├── include/byte/render/graph/
│       └── src/
├── shaders/         engine GLSL
├── samples/
│   ├── common/      sample framework (GLFW window, main loop) — not part of the library
│   └── 00_device_info/  01_headless_clear/  02_triangle/  03_window/  ...
├── tests/
│   ├── support/     capture log sink, GPU fixtures, image comparison
│   ├── core/        unit tests (no GPU)
│   ├── rhi/         GPU tests through the public API, incl. headless image tests
│   └── vulkan/      white-box backend tests (static builds only)
└── tools/           helper scripts (e.g. macOS Homebrew Vulkan setup)
```

### CMake targets
| Target (alias) | Links |
|---|---|
| `byte::core` | — |
| `byte::rhi` | `byte::core` PUBLIC; volk, VMA, Vulkan headers **PRIVATE** |
| `byte::render_graph` | `byte::rhi` PUBLIC |

Underlying names are `byte_render_<module>`. Each module has its own export macro
(`BYTE_RENDER_CORE_API`, `BYTE_RENDER_RHI_API`, `BYTE_RENDER_GRAPH_API`) so shared builds import/export
correctly across module boundaries.

## 4. Core building blocks

### 4.1 Handles — `core::Handle<Tag_>`
A non-owning, trivially copyable reference to an object stored in a pool: 32-bit index + 32-bit generation.

```cpp
template <class Tag_>
class Handle {
private:
    uint32_t index_ = k_invalid_index;
    uint32_t generation_ = 0;

public:
    constexpr Handle() noexcept = default;
    constexpr Handle(uint32_t index, uint32_t generation) noexcept;
    [[nodiscard]] constexpr bool is_valid() const noexcept;
    [[nodiscard]] constexpr uint32_t index() const noexcept;
    [[nodiscard]] constexpr uint32_t generation() const noexcept;
    friend constexpr bool operator==(Handle, Handle) noexcept = default;
};
```

- **Tag** = an empty, never-defined type that makes handles of different kinds distinct types at zero cost
  (`Handle<TextureTag>` and `Handle<BufferTag>` cannot be mixed up). Tags are declared by the module that owns
  the objects; `core` only provides the templates, so any future module can reuse them.
- The generation detects use-after-free: Debug builds validate it on every lookup; Release builds skip the check.

### 4.2 Owning resources — `core::Resource<Tag_, Deleter_>`
Move-only RAII owner that **publicly derives from `Handle<Tag_>`**, so an owner can be passed wherever a handle is
expected (copying out the handle is intended — it is a non-owning view).

```cpp
template <class Tag_, class Deleter_>
class Resource : public Handle<Tag_> {
private:
    [[no_unique_address]] Deleter_ deleter_; // empty deleters cost nothing

public:
    Resource(Handle<Tag_> handle, Deleter_ deleter) noexcept;
    ~Resource();                                // deleter_(handle) if valid
    Resource(Resource&& other) noexcept;        // `other` becomes invalid
    Resource& operator=(Resource&& other) noexcept;
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    [[nodiscard]] Handle<Tag_> release() noexcept; // gives up ownership
};
```

`Handle` has no virtual functions and is never deleted through a base pointer (it is a value type), so the
public inheritance is safe. **The deleter decides the destruction policy**:
- RHI resources use a deleter that *retires* the object to its device (deferred destruction, §6).
- Backend-internal Vulkan objects use `vulkan::Unique<VkType_, Deleter_>` (composition — `VkX` handles cannot be
  base classes) with immediate destruction, for long-lived objects destroyed while the device is idle.

### 4.3 Pools — `core::Pool<Value_, Tag_>`
Dense array of slots + free list + per-slot generation. `insert()` returns a `Handle<Tag_>`, `remove()` bumps the
generation, lookup is O(1) array indexing. No per-object heap allocation.

### 4.4 Other core pieces
`Result<T_>` / `Error` / `ErrorCode`, `BYTE_RENDER_ASSERT`, `Logger` + `LogSink` (an application-owned object,
not a global), `Flags<Enum_>`, `Extent2D/3D`, `Version`.

## 5. RHI object model

| Kind | Examples | Representation |
|---|---|---|
| Root objects | `InstanceFactory`, `Instance`, `Device` | Move-only classes holding a pointer to a forward-declared backend class (one per application; not hot) |
| Resources | `Texture`, `Buffer`, `Sampler`, `GraphicsPipeline`, `ComputePipeline` | `core::Resource<Tag_, DeviceReleaser>`: handle + device pointer (16 bytes) |
| Views | `TextureHandle`, `BufferHandle`, ... | `core::Handle<Tag_>` (8 bytes), used by command recording |
| Recording | `CommandList` | Move-only object bound to a device and queue |

```cpp
core::ConsoleLogSink sink;
core::Logger logger{sink, core::LogLevel::Info};

rhi::InstanceFactory factory{logger};                       // DI root: holds injected dependencies
auto instance = factory.create_instance({.enable_validation = true});  // Result<Instance>
auto device   = instance->create_device({});                // Result<Device>
auto albedo   = device->create_texture({...});              // Result<Texture>  (RAII)

rhi::TextureHandle view = *albedo;                          // non-owning, 8 bytes
const rhi::TextureInfo& info = device->info(view);          // data lives in the device's pool
```

- Creation only through the parent or a non-static factory (`InstanceFactory` holds the `Logger`).
- Resource data (descriptions, backend handles, allocations) lives in the device's pools; queries go through the
  device. Hot paths pass handles.
- A texture's pool index **is its bindless descriptor slot**, so shaders receive `handle.index()` directly.

## 6. Lifetime: CPU ownership vs. GPU lifetime
- **CPU lifetime**: the `Resource` object. Destroying it releases the handle to the device.
- **GPU lifetime**: the Vulkan object, its memory and its bindless slot; freed only when the GPU has finished all
  work that could reference it.

Mechanism — **timeline retirement**:
1. Each queue owns a timeline semaphore; every submission signals the next value.
2. A released handle is pushed to its pool's **typed retire list** with every queue's *last submitted* value
   (no `std::function`, no allocation per entry beyond the list's storage).
3. If the GPU has already completed those values, the object is freed immediately.
4. `collect()` frees every entry whose values are complete — on submit, frame begin, `wait()` and `wait_idle()`.
5. Device destruction waits for idle and frees everything.

Tagging with "last submitted" is conservative but cheap (no per-use tracking while recording) and correct under
bindless, where shaders may read any slot. Destroying a resource still referenced by an **unsubmitted** command
list is a programmer error.

## 7. Binding model — bindless only
- One global descriptor heap per device: sampled textures, storage textures, storage buffers, samplers.
- Resources are written into the heap once at creation; the slot equals the pool index.
- Per-draw data via push constants; larger data via buffers (bindless index or buffer device address).
- One global pipeline layout → no descriptor-set or layout management in the RHI.

## 8. Synchronization & submission
- `SyncPoint {queue, value}` names a point in a queue's submission stream; `submit()` returns one.
  `Device::is_complete()` polls, `Device::wait(point, timeout)` blocks; waiting for a never-submitted point is an error.
- v1: one universal queue (`QueueType::Graphics`). Data structures are sized by queue count for later
  async compute / transfer queues.
- Barriers are expressed as abstract **resource states** (`ShaderRead`, `ColorAttachment`, `DepthWrite`, `CopySrc`,
  `CopyDst`, `Present`, ...); the backend maps them to synchronization2 stages/access/layouts and batches them.
- Dynamic rendering (no render-pass/framebuffer objects).
- Frames in flight: 2 (configurable). Per-frame data uses persistently mapped ring buffers.

## 9. Threading
- Resource creation and destruction are thread-safe.
- `CommandList`s are recorded on any thread, one thread per list at a time, with per-thread command pools.
- Each queue is submitted from one thread at a time (internally synchronized).

## 10. Vulkan backend
- **volk** loads the Vulkan loader at runtime (no link-time Vulkan dependency).
  - Device-level functions go through a per-device `VolkDeviceTable` (no loader trampoline, no globals).
  - Instance-level function pointers are process-global in volk → **one live `Instance` per process**
    (creating a second returns an error). This is the only global state in the engine.
  - Loader search: next to the executable (shipped apps), then the system, `$VULKAN_SDK`, Homebrew (macOS).
- Memory: Vulkan Memory Allocator, one allocator per device.
- Required adapter features: Vulkan 1.3, `dynamicRendering`, `synchronization2`, `timelineSemaphore`, the descriptor
  indexing set for bindless, `bufferDeviceAddress`, `scalarBlockLayout`, `samplerAnisotropy`; a queue family with
  graphics + compute. `AdapterInfo` reports unsupported adapters with a reason.
- Portability: `VK_KHR_portability_enumeration` / `VK_KHR_portability_subset` enabled when exposed (MoltenVK).
- Presentation (optional): `VK_KHR_surface` + platform surface extensions on the instance, `VK_KHR_swapchain`
  on the device.
- Validation: `VK_LAYER_KHRONOS_validation` + debug messenger routed into the `Logger`; optional (falls back with a
  warning if missing). Objects get debug names when validation is on.

## 11. Headless and presentation
- A `Device` never requires a window. Rendering targets are textures; results are read back to CPU memory.
- Headless image tests (render → read back → compare) validate every rendering feature, also in CI without a GPU
  display (Linux + lavapipe).
- Presentation is an add-on: `Swapchain` created from an API-agnostic `NativeWindow` (HWND / X11 / Wayland /
  `CAMetalLayer`). The engine never owns the window; samples use GLFW.

## 12. Render graph (v1 scope)
- Passes declare reads/writes of transient or imported resources.
- Compile: cull unused passes, order, compute barriers, allocate transient resources from a pool.
- Execute: record into RHI command lists.
- Later: transient memory aliasing, async compute, multi-queue.

## 13. Shaders
- GLSL compiled to SPIR-V at build time (`glslangValidator`), embedded or loaded as files.
- Bindless access via `GL_EXT_nonuniform_qualifier`; push constants with scalar layout.

## 14. Conventions
- Depth range [0, 1], **reverse-Z** by default (clear to 0, `GREATER` compare, infinite far plane supported).
- Right-handed world space, Y-up. Clip-space conventions are fixed by the RHI so all backends match.

## 15. Errors and logging
- No exceptions; `Result<T_>` for runtime failures, `BYTE_RENDER_ASSERT` for programmer errors (Debug).
- `Logger` is injected through `InstanceFactory`; it formats with `std::format` and captures `std::source_location`.

## 16. Build, dependencies, options

| Dependency | Use | Visibility |
|---|---|---|
| Vulkan SDK / Homebrew Vulkan (loader, MoltenVK, validation layers, glslang) | runtime, shader build | system install |
| Vulkan-Headers (pinned) | Vulkan backend | private |
| volk (pinned) | Vulkan function loading | private |
| Vulkan Memory Allocator (pinned) | GPU memory | private |
| GLFW (pinned) | samples only | — |
| doctest (pinned) | tests only | — |

| Option | Default | Meaning |
|---|---|---|
| `BYTE_RENDER_RHI_BACKEND` | `Vulkan` | Compile-time backend |
| `BYTE_RENDER_BUILD_SHARED` | `OFF` | Shared instead of static libraries |
| `BYTE_RENDER_ASSERTS` | `AUTO` | `AUTO` = Debug only, `ON`, `OFF` |
| `BYTE_RENDER_WARNINGS_AS_ERRORS` | `ON` | Warnings are errors in ByteRender targets |
| `BYTE_RENDER_LTO` | `ON` | Link-time optimization in Release |
| `BYTE_RENDER_BUILD_SAMPLES` / `_TESTS` | `ON` when top-level | Samples / tests |

## 17. Testing
- `byte_render_tests`: unit tests, no GPU.
- `byte_render_gpu_tests` (label `gpu`): public API on a real GPU, including headless image tests.
- `byte_render_vulkan_tests` (label `gpu`, static builds): white-box backend tests.
- Every GPU test fails if anything logs a warning or error. Sanitizer presets: `asan` (ASan+UBSan), `tsan`.
