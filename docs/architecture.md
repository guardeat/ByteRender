# Bytee — Architecture

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
 bytee::render::graph   passes, transient resources, automatic barriers       (knows only the RHI)
 ──────────────────────────────────────────────────────────────────────────────
 bytee::render::rhi     Instance, Device, CommandList, Swapchain   (objects)
                       Texture, Buffer, ... = core::Resource<Tag_, Deleter_>   (RAII owners)
                       TextureHandle, ...   = core::Handle<Tag_>               (8-byte views)
 ──────────────────────────────────────────────────────────────────────────────
 Vulkan backend        pools, volk device tables, VMA, retire lists, bindless heap   (private)
 ──────────────────────────────────────────────────────────────────────────────
 bytee::core            Result, assert, Logger, Flags, Handle, Resource, Pool          (no GPU code)
```

Rule: nothing above the backend line sees a Vulkan type.

## 3. Repository layout

```
Bytee/
├── CMakeLists.txt  CMakePresets.json  CLAUDE.md  README.md
├── .clang-format  .clang-tidy  .editorconfig  .gitignore
├── cmake/
│   ├── bytee_options.cmake        BYTEE_* options
│   ├── bytee_compiler.cmake       warnings, sanitizers, LTO
│   ├── bytee_dependencies.cmake   pinned FetchContent dependencies
│   └── bytee_shaders.cmake        GLSL → SPIR-V
├── docs/            architecture.md  roadmap.md
├── modules/
│   ├── core/
│   │   ├── include/bytee/core/           public headers
│   │   └── src/
│   ├── rhi/
│   │   ├── include/bytee/render/rhi/     public, API-agnostic headers
│   │   └── src/
│   │       ├── common/                  backend-agnostic internals
│   │       └── vulkan/                  the only place that includes Vulkan / volk / VMA
│   └── render_graph/
│       ├── include/bytee/render/graph/
│       └── src/
├── shaders/         engine GLSL
├── samples/
│   ├── common/      sample framework (GLFW window, main loop) — not part of the library
│   └── 00_device_info/  01_headless_clear/  02_triangle/  03_window/  ...
├── benchmarks/      micro-benchmarks (built, not run as tests)
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
| `bytee::core` | — |
| `bytee::rhi` | `bytee::core` PUBLIC; volk, VMA, Vulkan headers **PRIVATE** |
| `bytee::render_graph` | `bytee::rhi` PUBLIC |

Underlying names are `bytee_<module>`. Each module has its own export macro
(`BYTEE_CORE_API`, `BYTEE_RHI_API`, `BYTEE_GRAPH_API`) so shared builds import/export
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
- Fixed maximum capacity chosen at construction (fits bindless: the descriptor heap is fixed-size too). Storage is
  allocated lazily in chunks of 256 slots and **never moves**: references from `get()` stay valid while other objects
  are inserted.
- Slot generation: odd = alive, even = free. `remove()` bumps it, so stale handles are rejected by `contains()` /
  `try_get()`; `get()` asserts in Debug and is unchecked in Release. Freed slots are reused LIFO (cache-warm).
- O(1) everything, no hashing, no per-object allocation; `insert()` returns an invalid handle when full.
- Threading: `insert()`/`remove()` need external synchronization (the owner holds a lock); lookups may run
  concurrently with insert/remove of other slots.
- Measured (Apple M5 Pro, Release + LTO, 65 536 objects, random order): lookup 0.59 ns (raw `std::vector` 0.54 ns,
  `std::unordered_map` 1.74 ns); insert + remove 1.48 ns per operation (`unordered_map` 7.66 ns).
  Benchmark: `benchmarks/pool_benchmark.cpp`.

### 4.4 Other core pieces
`Result<T_>` / `Error` / `ErrorCode`, `BYTEE_ASSERT`, `Logger` + `LogSink` (an application-owned object,
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
- Implementation: `ResourceReleaser` (the deleter) holds the device's opaque `detail::DeviceImpl*`; opaque backend
  types are declared at namespace scope (`detail::InstanceImpl`, `detail::DeviceImpl`) so public headers can name them.
- Buffers: `MemoryLocation::Device | Upload | Readback`; every buffer has a GPU address (`info.device_address`);
  Upload/Readback buffers are persistently mapped and accessed with `Device::write()` / `read()`.
- Textures: 2D (with array layers) and 3D, explicit mip count; a default image view (only for usages that need one); format/usage support is checked
  against the GPU (`NotSupported`); invalid descriptions return `InvalidArgument`.
- Pool capacities come from `DeviceDesc::capacities`; a full pool returns `ErrorCode::LimitExceeded`.

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

Implementation: `vulkan::Queue` (VkQueue + timeline semaphore, returns raw `VkResult`s so the device can mark itself
lost), `detail::RetireList<Item_, QueueCount_>` (typed, thread-safe FIFO; the destroy function is passed to
`collect()`, runs outside the lock one item at a time — no allocation, re-entrant), `Device::Impl::retire()` /
`collect()`. Backend-internal Vulkan objects use a `RetireList<RetiredObject>`; each resource pool gets its own list.

Tagging with "last submitted" is conservative but cheap (no per-use tracking while recording) and correct under
bindless, where shaders may read any slot. Destroying a resource still referenced by an **unsubmitted** command
list is a programmer error.

## 7. Binding model — bindless only
- **One descriptor set per resource kind**, each a single update-after-bind, partially bound array at binding 0:
  set 0 sampled textures, set 1 storage textures, set 2 samplers. Buffers need no descriptors: shaders access them by
  **buffer device address**.
- **Adaptive capacity**: each set is sized to min(requested, the largest count the GPU confirms through
  `vkGetDescriptorSetLayoutSupport`), also bounded by the per-stage update-after-bind limits. MoltenVK confirms ~1200
  descriptors per set (Apple M5 Pro: 1211 textures, 1211 samplers) despite reporting 1M update-after-bind limits;
  desktop drivers confirm the full defaults (65536 textures, 4096 samplers). The texture/sampler pools shrink to match
  (slot = pool index); `Device::capacities()` reports the effective capacities.
- Resources are written into their set once at creation (`rhi::bindless_index(handle)` = pool index); slots are reused
  only after deferred destruction, so pending GPU work never sees a slot change.
- Shaders: `#include "bytee/bindless.glsl"` → `bytee_sample_2d(texture_index, sampler_index, uv)` and friends.
  Sample textures in the `ShaderRead` or `DepthRead` state (both use the read-only layout the descriptors are written
  with). UV (0, 0) is the top-left texel (row 0 of the data). Storage textures: `bytee_image_load_2d` /
  `bytee_image_store_2d` (declared without a format qualifier, so the texture's own format applies), accessed in the
  `ShaderWrite` state. Shadow maps: `bytee_sample_2d_shadow` with a comparison sampler.
- The sets are bound once per command list (graphics and compute) and stay bound: one global pipeline layout
  (3 sets + 128 bytes of push constants) → no descriptor management in the RHI.

## 8. Synchronization & submission
- `SyncPoint {queue, value}` names a point in a queue's submission stream; `submit()` returns one.
  `Device::is_complete()` polls (false for never-submitted points and on a lost device); `Device::wait(point, timeout)`
  blocks and then collects retired objects. Errors: `Timeout` (allocation-free, so polling with short timeouts is
  cheap), `InvalidArgument` for a never-submitted point (instead of a deadlock), `DeviceLost`.
- v1: one universal queue (`QueueType::Graphics`). Data structures are sized by queue count for later
  async compute / transfer queues.
- Barriers are expressed as abstract **resource states** (`ShaderRead`, `ShaderWrite`, `ColorAttachment`, `DepthWrite`,
  `DepthRead`, `CopySrc`, `CopyDst`, `VertexBuffer`, `HostRead`, ...); one table maps them to synchronization2 stages, access and
  layouts. GPU writes the CPU reads back (Readback buffers) need a barrier into `HostRead` before submission ends
  (a completed submission alone does not make them visible to the host). The RHI does not track states: barriers name `before` and `after` explicitly (the render graph computes
  them). `CommandList::barrier()` records one `vkCmdPipelineBarrier2` per 32 barriers from stack storage — no allocation.
- **Command lists**: `Device::begin_commands()` → record → `Device::submit(std::move(list))` → `SyncPoint`. Each list
  owns a recycled command context (its own command pool + primary command buffer), so lists record on any thread in
  parallel without locks and without thread-local state; contexts are reused once their submission has completed, and
  a list destroyed without submitting simply returns its context. Commands: barriers, fill/copy buffer, buffer ↔
  texture copies (tightly packed texel data).
- **Render passes** (dynamic rendering): `begin_render_pass({.colors, .depth})` / `end_render_pass()`; up to 8 color
  attachments + depth(-stencil); `LoadOp` Load/Clear/DontCare, `StoreOp` Store/DontCare; depth clears to 0 by default
  (reverse-Z). Attachments render into mip 0 / layer 0 through a dedicated single-mip attachment view (other mips and
  layers: backlog). Barriers and copies inside a render pass assert.
- **Downloads**: `Device::download(texture, data, {region, before, after})` reads a region back into caller memory
  (blocking; screenshots, tests, tools).
- **Uploads**: `Device::upload(buffer | texture, data, ...)` = RAII staging buffer + copy + barriers + submit; the
  staging buffer is freed by deferred destruction once the copy completes. Buffer uploads use conservative global
  barriers (later work on the queue needs no barrier); texture uploads take `before` / `after` states. Upload sizes and
  regions are validated at runtime (`InvalidArgument`); command-list copies only assert (hot path).
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
  - `Instance` creation: Vulkan 1.3 loader required; portability enumeration when exposed; `VK_KHR_surface` plus every
    available platform surface extension (`Instance::is_presentation_supported()`); validation optional — a missing or
    broken layer logs one warning and creation continues without it.
- Memory: Vulkan Memory Allocator, one allocator per device (functions fetched through volk).
- Vulkan objects are created into local handles and stored in their owner only on success (outputs are undefined
  when a create call fails, and owners destroy whatever handles they hold).
- `Device` creation: best supported adapter (discrete > integrated > virtual > others, then most device-local memory)
  or an explicit index; one universal queue; per-device `VolkDeviceTable`; `VK_KHR_swapchain` only when the instance
  supports presentation; several devices per instance allowed.
- Destruction order is enforced: a parent destroyed while children are alive reports each child by type and debug
  name and asserts in Debug; in Release the parent is deliberately leaked instead of freed (a leak is recoverable,
  a use-after-free is not).
- Required adapter features: Vulkan 1.3, `dynamicRendering`, `synchronization2`, `timelineSemaphore`, the descriptor
  indexing set for bindless, `bufferDeviceAddress`, `scalarBlockLayout`, `samplerAnisotropy`,
  `shaderStorageImageRead/WriteWithoutFormat` (format-less bindless storage images); a queue family with
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
- Presentation is an add-on: `Device::create_swapchain({.window = NativeWindow, .extent})`. `NativeWindow` describes a
  window without window-system types (Metal layer, Win32, Xlib/Xcb/Wayland — Linux in step 18 — or **Headless**, which
  makes swapchains testable without a display). The engine never owns windows; samples use GLFW via `samples/common`.
- Frame loop: `swapchain.acquire()` (throttles to `frames_in_flight`, default 2; recreates on resize / out-of-date;
  returns an invalid frame for a zero-size window) → record (image: Undefined → … → `Present`) →
  `device.present(swapchain, std::move(list))` → `SyncPoint`. Swapchain images are external `TextureHandle`s (views
  owned by Bytee, images by the swapchain). Acquire semaphores per frame slot, present semaphores per image.
  Recreation waits for the device to idle (rare). `Undefined` as a barrier source waits for all prior commands so the
  first transition chains with the acquire semaphore wait.

## 12. Render graph (v1 scope)
- Namespace `bytee::render` (module `bytee::render_graph`, headers under `bytee/render/graph/`).
- A **pass is a class** satisfying the `RenderPass` concept: `setup(PassBuilder&)` declares resource use,
  `execute(PassContext&)` records commands. No base class, no virtual calls: the graph stores a pointer plus one
  function pointer per pass. Passes are long-lived and own their pipelines (constructor = init, destructor = release,
  through RAII); the graph is rebuilt every frame and only refers to them, so a pass must outlive the graph's execution.
- `graph.add_pass("Name", pass)` runs `setup()` immediately, so a pass's outputs feed the next pass. Passes execute in
  the order they are added; unused ones are culled at compile time.
- Resources: `TextureRef` / `BufferRef` (4-byte indices, valid until `reset()`). Transient ones come from
  `builder.create_texture/create_buffer("name", desc)` — usage flags are derived from their accesses; imported ones
  from `graph.import_texture/import_buffer("name", handle, {.before, .after})`.
- Accesses: `builder.read/write(ref, state)` with RHI resource states (one state per resource per pass, checked);
  render targets through `builder.color_attachment/depth_attachment` (the graph begins and ends the render pass).
- Storage is flat arrays plus one name buffer, cleared but not freed by `reset()`: rebuilding every frame does not
  allocate once warm. `graph.pass(i)` / `texture(ref)` / `buffer(ref)` give read-only views (compile, dumps, tests).
- **Compile** (`graph.compile()`, CPU only; `record()` runs it when the graph changed):
  - Culling, last pass first: a pass executes if it writes an imported resource or something a later executing pass
    uses. Culled passes never execute and their transients are never created.
  - Validation (`InvalidArgument`, with pass and resource names): reading a transient before any pass wrote it; a
    transient in a state only imported resources can be in (`Present`, `HostRead`); attachments of one pass with
    different sizes; an imported resource created without the usage its state needs.
  - Transient usage flags = union over their accesses (`ShaderRead` → Sampled, `ShaderWrite` → Storage, ...).
  - Barriers: per resource the graph tracks the current state and whether the last access wrote. A barrier is placed
    when the state changes or a write is involved; repeated reads in one state need none. Imported resources start in
    `before` (earlier work counts as a write) and always end in `after`, even when no executing pass used them.
- **Record / execute**: `graph.record()` → one `rhi::CommandList` (barriers, render passes for passes with attachments,
  each pass's `execute()`); submit it, or present it for swapchain frames. `graph.execute()` = record + submit. Once
  per `reset()`. `PassContext` gives the command list, the device and the GPU handles behind refs.
- **Tooling**: every executing pass is recorded inside a debug label with its name (`CommandList::begin_debug_label`
  / `end_debug_label`, visible in Xcode / RenderDoc; no-op without validation, no allocation). `graph.to_graphviz()`
  returns the graph in DOT format: passes in order, resources (imported ones bold), edges labeled with states, culled
  passes greyed out.
- **Transient pool**: transients survive `reset()` and are reused by a later frame with the same shape and usage;
  entries unused for `k_transient_keep_frames` (4) frames are released (deferred destruction). Reuse across frames is
  safe on one queue: a transient's first barrier (from `Undefined`) waits for all earlier work.
- Later: transient memory aliasing, async compute, multi-queue.

## 13. Shaders and pipelines
- GLSL compiled to SPIR-V at build time: `bytee_add_shaders(target SOURCES a.vert b.frag)` runs `glslangValidator`
  (silent, `-g` in Debug, includes tracked through depfiles) and embeds the result as
  `shaders::a_vert` (`std::array<std::uint32_t, N>`) in a generated header — no shader files at runtime.
- `GraphicsPipeline` = `core::Resource` (deferred destruction like every resource). Dynamic rendering formats, no
  vertex input state (**vertex pulling**: shaders compute vertices or read them by buffer device address via
  `GL_EXT_buffer_reference`), dynamic viewport/scissor, cull/front face, reverse-Z depth state, blend modes.
- One global pipeline layout: the 3 bindless sets + 128 bytes of push constants visible to every stage.
  `CommandList::push_constants(value)` for any trivially copyable value of at most 128 bytes (checked at compile time).
- `ComputePipeline` = `core::Resource`, described by its shader alone (`ComputePipelineDesc`). Dispatch outside render
  passes: `bind_pipeline(compute)`, `push_constants`, `dispatch(x, y, z)`; `rhi::dispatch_group_count(items, group_size)`
  sizes it. Resources are reached exactly as in graphics: bindless indices and buffer device addresses.
- Draws: `bind_pipeline`, `push_constants`, `set_viewport`/`set_scissor` (set automatically by `begin_render_pass`),
  `bind_index_buffer`, `draw`, `draw_indexed`.

## 14. Conventions
- Depth range [0, 1], **reverse-Z** by default (clear to 0, `GREATER` compare, infinite far plane supported).
- Right-handed world space, Y-up. Clip space is **Y-up** (like D3D/OpenGL): the Vulkan backend uses a negative-height
  viewport, which also makes `FrontFace::CounterClockwise` mean counter-clockwise in Y-up clip space. Verified by image
  tests (triangle orientation, culling with both windings).

## 15. Errors and logging

### Two kinds of failure
| Kind | Example | Mechanism | Release builds |
|---|---|---|---|
| Runtime failure (can happen in a correct program) | out of GPU memory, no supported adapter, device lost, timeout | `core::Result<T_>` returned to the caller | always active |
| Programmer error (bug in the calling code) | empty object used, invalid handle, out-of-range index | `BYTEE_ASSERT(cond, "msg {}", args...)`: prints condition, message, location, then aborts | compiled out |

- `Error` is 16 bytes: an inline `ErrorCode` plus a pointer to optional details (message, location).
  `Error{ErrorCode::Timeout}` never allocates (cheap enough for polling loops); `make_error(code, "fmt {}", args...)`
  allocates once, on the failure path only. Accessors: `code()`, `message()`, `location()`.
- `Result<T_>` holds a value or an `Error` (`Result<uint64_t>` ≤ 24 bytes); `Result<void>` for operations without a
  value. `[[nodiscard]]`. `BYTEE_TRY(expr)` returns a failed result's error from the enclosing function.
- **Error locations are Debug-only**: `Error::location` always exists (stable layout across configurations) but is
  only filled when `BYTEE_ERROR_LOCATIONS` is on (default: Debug), so Release binaries contain no file names.
- **The engine returns errors; it does not log them.** The caller decides how to report. The engine itself only
  logs fallback warnings (e.g. validation unavailable), leak reports and graphics-API validation messages.
- **Device lost is sticky**: the first `DeviceLost` marks the device as lost; every later fallible call returns
  `ErrorCode::DeviceLost` immediately without touching the GPU. The application recovers by destroying and
  recreating the device.
- Assert failures print and abort (no hookable global handler — no globals). A debugger stops at the failing line.
  Disabled asserts evaluate neither the condition nor the message arguments (and cause no unused warnings).
- Source locations (asserts, errors, logs) use paths relative to the source tree (`-fmacro-prefix-map`), so no
  absolute build paths end up in binaries or output.

### Logging
- `LogSink`: user-implementable, thread-safe destination (console, file, editor panel). `ConsoleLogSink` provided.
- `Logger`: application-owned object (not a global), injected through `InstanceFactory`. Runtime level check is a
  single atomic load; text is formatted with `std::format` only when the level is enabled; the caller's
  `std::source_location` is captured automatically.
- **Compile-time stripping**: `BYTEE_LOG_LEVEL` (default `Debug` in Debug builds, `Info` otherwise). Calls
  below it compile to nothing (`if constexpr`): no runtime check, and optimized builds contain no format string
  (verified). Argument expressions are still evaluated — keep expensive work out of trace/debug calls or guard it
  with `core::is_log_level_compiled(level)`.
- Graphics-API messages (validation layer, loader) are routed into the same logger.

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
| `BYTEE_RHI_BACKEND` | `Vulkan` | Compile-time backend |
| `BYTEE_BUILD_SHARED` | `OFF` | Shared instead of static libraries |
| `BYTEE_ASSERTS` | `AUTO` | `AUTO` = Debug only, `ON`, `OFF` |
| `BYTEE_ERROR_LOCATIONS` | `AUTO` | Record where errors are created: `AUTO` = Debug only, `ON`, `OFF` |
| `BYTEE_LOG_LEVEL` | `AUTO` | Compile-time minimum log level: `AUTO` = `Debug` in Debug, `Info` otherwise |
| `BYTEE_SANITIZE` | empty | `address` (ASan+UBSan) or `thread` (TSan), top-level builds |
| `BYTEE_WARNINGS_AS_ERRORS` | `ON` | Warnings are errors in Bytee targets |
| `BYTEE_LTO` | `ON` when top-level | Link-time optimization in Release (consumers opt in: GCC LTO static libraries contain only bitcode) |
| `BYTEE_BUILD_SAMPLES` / `_TESTS` / `_BENCHMARKS` | `ON` when top-level | Samples / tests / benchmarks |

## 17. Testing
- `bytee_tests`: unit tests, no GPU.
- `bytee_gpu_tests` (label `gpu`): public API on a real GPU, including headless image tests.
- `bytee_vulkan_tests` (static builds): white-box backend tests; cases that need a GPU are labelled `gpu`.
- Every GPU test fails if anything logs a warning or error. Sanitizer presets: `asan` (ASan+UBSan), `tsan`.
