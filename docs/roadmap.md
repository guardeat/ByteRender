# Bytee — Roadmap

One step per prompt. Each step: plan → implement → from-scratch build, tests, run (zero noise) → one commit.
Rendering features are validated **headless first** (image tests), presentation comes after.

## Phase 0 — Foundation
- [x] **Step 0 — Rules & design.** `CLAUDE.md`, `docs/architecture.md`, `docs/roadmap.md`.
- [x] **Step 1 — Build system.** Repo hygiene files, CMake presets (debug/release/asan/tsan/shared), options,
  warnings-as-errors, sanitizers, LTO, module skeletons with export macros, doctest harness,
  `00_device_info` sample printing the version.
- [x] **Step 2 — Core basics.** `Result<T_>`, `Error`, `BYTEE_ASSERT`, `LogSink`/`Logger`, `Flags<Enum_>`,
  `Extent2D/3D`, `Version`. Unit tests.
- [x] **Step 3 — Handles, resources, pools.** `Handle<Tag_>`, `Resource<Tag_, Deleter_>`, generational
  `Pool<Value_, Tag_>`. Unit tests (generation checks, moves, release) + micro-benchmark of pool operations.

## Phase 1 — RHI + Vulkan, headless first
- [x] **Step 4 — Instance.** `InstanceFactory` → `Instance`; volk + loader search, Vulkan 1.3, portability,
  validation into the logger, adapter enumeration with requirement checks. Sample lists GPUs.
- [x] **Step 5 — Device.** Adapter selection, universal queue, `VolkDeviceTable`, VMA, leak tracking,
  debug names. GPU tests with the zero-noise fixture.
- [x] **Step 6 — Timeline & retirement.** Timeline queues, typed retire lists, `SyncPoint`, `wait`/`is_complete`/
  `wait_idle`. Unit + white-box tests, sanitizer runs.
- [x] **Step 7 — Buffers & textures.** Pools + handles + `Resource` deleters, VMA allocations, deferred release,
  `info()` queries, CPU access to host-visible buffers. (Staging uploads moved to step 8: they need copy commands.)
- [x] **Step 8 — Command lists & barriers.** Per-thread command pools, `CommandList`, resource-state barriers
  (batched sync2), `submit()` → `SyncPoint`, copies, staging uploads (buffer → buffer / texture).
- [x] **Step 9 — Headless clear & readback.** Dynamic rendering clear into a texture, readback, image-compare test
  helper. `01_headless_clear` sample writes a PNG-free raw/PPM image.
- [x] **Step 10 — Shaders & graphics pipeline.** CMake GLSL → SPIR-V, `GraphicsPipeline`, push constants.
  **Headless triangle** image test.
- [x] **Step 11 — Bindless.** Global descriptor heap (slot = pool index), samplers. Textured quad image test.
- [x] **Step 12 — Window & swapchain.** `NativeWindow`, `Swapchain`, frames in flight, resize, present.
  `03_window` sample (GLFW): **triangle on screen**.
- [x] **Step 13 — Compute.** `ComputePipeline`, dispatch, storage resources. Compute image test.

## Phase 2 — Render graph
- [x] **Step 14 — Graph API.** Pass builder, transient + imported resources.
- [x] **Step 15 — Compile & execute.** Culling, ordering, automatic barriers, transient resource pool;
  samples ported to the graph.
- [x] **Step 16 — Graph tooling.** Pass markers, Graphviz dump, validation.

## Phase 3 — Usability & portability
- [x] **Step 17 — Debug tooling.** GPU markers/labels (with step 16), ImGui integration sample.
- [ ] **Step 18 — Windows & Linux.** Builds on all platforms; CI matrix with headless GPU tests (lavapipe).
  Done, verified on macOS only: Linux surfaces (Xlib / XCB / Wayland, no window-system headers), Linux window
  platforms in samples/common (03-05), MSVC support (relative source paths via `/d1trimfile`, no abort dialogs,
  `asan` preset without `/RTC1` and incremental linking), platform-independent path checks in tests.
  Open: verify on Windows and Linux; CI (GitHub Actions workflow drafted: Linux GCC + Clang/ASan with lavapipe,
  Windows MSVC debug + ASan without GPU, macOS with MoltenVK; pushing it needs the `workflow` token scope).
- [ ] **Step 19 — Packaging.** `install()` + CMake package config: `find_package(Bytee)` / FetchContent.

## Backlog
- Null backend for GPU-less unit tests of RHI logic
- Transient memory aliasing, async compute and transfer queues
- Rendering into mip levels / array layers other than 0 (attachment view cache)
- Pipeline cache (persisted across runs) and asynchronous pipeline creation
- Math library decision (own vs. glm)
- Scene / mesh / material layer on top of the graph
- Additional backends (Metal / D3D12)

## Open decisions
- (none)
