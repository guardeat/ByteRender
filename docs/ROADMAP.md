# ByteRender — Roadmap

One step per session/prompt. Each step ends building + running, then one commit.
Steps may be split further when we get to them.

## Phase 0 — Foundations
- [x] **Step 0 — Rules & design docs.** `CLAUDE.md`, `docs/ARCHITECTURE.md`, `docs/ROADMAP.md`. Git repo.
- [x] **Step 1 — Toolchain & CMake skeleton.**
  Install CMake. Repo layout from ARCHITECTURE §3, `.gitignore`, `.clang-format`,
  `.editorconfig`, build options, warnings-as-errors, empty `core`/`rhi`/`render_graph` targets,
  per-module export macros, a `hello` sample that builds and runs on macOS (static + shared).
- [x] **Step 2 — Core module.**
  `Result<T>` / `Error`, `BYTE_RENDER_ASSERT`, log sink interface,
  basic types (`Extent2D/3D`), `Flags<E>`. doctest set up with first unit tests.

## Phase 1 — RHI + Vulkan bring-up
- [x] **Step 3 — Instance.** `InstanceFactory` → `rhi::Instance` (pimpl) → Vulkan instance via volk, portability enumeration
  for MoltenVK, validation layers + debug messenger routed to the log sink, adapter enumeration.
  Sample prints available GPUs.
- [x] **Step 4 — Device.** Adapter selection, logical device, queues, feature checks
  (Vulkan 1.3, dynamic rendering, sync2, descriptor indexing, timeline semaphores), VMA, live-object tracking.
- [x] **Step 5 — Timeline & retire queue.** Per-queue timeline semaphore, submit/wait, retire queue + `collect()`,
  `wait_idle()`. Unit-test the retire logic.
- [ ] **Step 6 — Window surface, swapchain, frame loop.** `NativeWindow`, `Swapchain`, frames in flight,
  resize handling, `CommandList` basics, resource-state barriers. **Clear screen to a color** (GLFW sample).
- [ ] **Step 7 — Buffers & textures.** `Buffer`, `Texture`, `TextureView`, `Sampler`, staging uploads,
  debug names. Verify deferred destruction under load.
- [ ] **Step 8 — Shaders & graphics pipeline.** CMake GLSL→SPIR-V, `ShaderModule`, `GraphicsPipeline`,
  push constants. **First triangle.**
- [ ] **Step 9 — Bindless heap.** Global descriptor heap, slot allocator (timeline-retired),
  global pipeline layout. **Textured quad** sampled by index.
- [ ] **Step 10 — Compute.** `ComputePipeline`, dispatch, storage buffers/textures by bindless index.

## Phase 2 — Render graph
- [ ] **Step 11 — Graph API.** Pass builder, resource declarations (transient + imported), handles.
- [ ] **Step 12 — Compile & execute.** Pass culling, ordering, automatic barriers, transient resource pool.
  Port the samples to the graph.
- [ ] **Step 13 — Graph debugging.** Pass markers/labels, graph dump (Graphviz), validation.

## Phase 3 — Usability & portability
- [ ] **Step 14 — Debug tooling.** GPU markers, RenderDoc-friendly names, optional ImGui integration sample.
- [ ] **Step 15 — Windows & Linux.** Verify builds/runs on all platforms; CI matrix (decide later).
- [ ] **Step 16 — Packaging.** `install()` + CMake package config so other projects can
  `find_package(ByteRender)` or use it via FetchContent/submodule.

## Later / backlog
- Null backend for GPU-less tests
- Transient memory aliasing, async compute
- Math library decision (glm vs. own)
- Mesh/scene/material layer on top of the graph
- Additional backends (Metal / D3D12) — would revisit compile-time vs. runtime selection

## Open decisions
- (none)
