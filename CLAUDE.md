# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A learning-oriented, modular **DirectX 12** renderer in **C++20 + CMake**, Windows-only. Three build targets:

- `DX12Engine` — the engine, built as a **static library** (`src/DX12Engine/`).
- `DemoScene` — example executable that links the engine (`DemoScene/`); contains `main` and all app/scene wiring.
- `AssetCooker` — offline CLI that cooks raw assets into runtime form (`tools/AssetCooker/`); a single `.cpp`.

There is **no test suite** — verify changes by building and running `DemoScene`.

For a deep, authoritative walkthrough of the rendering pipeline — including known bugs and their fixes — read `docs/Rendering-Architecture.md`. Consult it before making non-trivial rendering changes.

## Build & run

CMake fetches all third-party deps on first configure (needs internet): DirectX-Headers, DirectXTex, tinyobjloader, tinygltf, meshoptimizer, nlohmann_json, FreeType, RmlUi, and (Debug only) Dear ImGui.

```bash
cmake -S . -B build                              # configure (first run fetches deps)
cmake --build build --config Debug               # everything: engine + DemoScene + AssetCooker + CookAssets
cmake --build build --target DemoScene --config Debug   # just the demo (pulls in its deps)
```

Run: `build/DemoScene/Debug/DemoScene.exe`. Post-build steps auto-copy cooked `res/`, all `.hlsl`/`.hlsli` shaders, and `dxcompiler.dll`/`dxil.dll` next to the exe — launch directly, no manual copying.

Manual asset cook (normally automatic via the `CookAssets` target):
```bash
build/tools/AssetCooker/Debug/AssetCooker.exe --in res --out build/res [--force] [--clean-cache]
```

**Asset flow:** raw assets live in `res/`; `CookAssets` copies+cooks them into `build/res/` (DDS+mips, GLB texture extraction, mesh LODs, `materials.json`/`lods.json` manifests, incremental via `asset_cooker.cache`); `DemoScene` then stages `build/res/` next to its exe. Newly added textures/models under `res/` are discovered and cooked on the next build. `LoadCookedModel(id)` reads this cooked output — a model must be cooked before it will load at runtime.

**Config gates:** ImGui debug UI compiles **only in Debug** (`DX12ENGINE_ENABLE_IMGUI_DEBUG_UI`, defaulted per generator in the root `CMakeLists.txt`). Shader hot-reload (`ResourceManager::ReloadChangedShaders`) is a Debug-build feature.

## Architecture

### Entry point & frame loop
`DX12Engine::Launcher::Launch(Application*, windowSize[], name)` (`src/DX12Engine/Launcher.h`) creates a `RenderContext`, calls `app->Init(...)`, then spins a window-message loop calling `Application::Update(ts, elapsed)` each frame. To build your own app, subclass the abstract `Application` (Init/Update/HandleWindowEvent) — `DemoScene/src/DX12EngineDemoApp.cpp` is the reference implementation.

### Data-driven render pass graph (the core abstraction)
The renderer is a **frame-graph-lite**. You describe passes declaratively and the renderer auto-wires producers to consumers:

- `PipelineBuilder` (`.AddPass` / `.AddPassIf`) assembles a `RenderPipelineConfig` — an ordered list of `RenderPassConfig`. See `DX12EngineDemoApp::Init` for the canonical build.
- Each `RenderPassConfig` declares its `Type`, `Writes` (`PipelineResource`s it produces), `ResourceBindings` (resources it consumes, mapped to shader slots), and `InputResources`.
- `Renderer::CreateRenderPipeline` resolves the graph: it wires each pass's inputs to prior passes' outputs via a producer map, injects fallbacks (env map, reactive mask, CSM CB), instantiates the `RenderPass` objects, and computes transient descriptor layout.
- `Renderer::ExecutePipeline` runs passes in order every frame; `Renderer::OnResize` re-creates every transient target and remaps bindings.

The demo pipeline order: `ShadowMap → CubeShadowMap → CascadedShadowMap → Geometry (G-buffer) → Lighting (clustered deferred PBR+IBL) → SSR → TAA (optional) → Transparent → UI`, followed by `PresentFrame` (a full-screen blit running `FinalRender_PS`: exposure → ACES → gamma, then optional FXAA). It's a **deferred core with a forward transparent tail** — transparents get analytic clustered lights but no shadows, SSR or TAA.

The enums that define this vocabulary live in `RenderPipelineConfig.h`: `RenderPassType`, `PipelineResource`, `ResourceSlot`, `InputResourceType`.

### Adding a render pass
1. Add a value to `RenderPassType` (and any new `PipelineResource`/`ResourceSlot`/`InputResourceType`).
2. Implement a `RenderPass` subclass in `src/DX12Engine/Rendering/RenderPass/` (override `Init`, `Execute`, `CreatePSO`, `GetRenderTarget`, `OnResize`).
3. Register it in `Renderer::CreateRenderPass` (the pass factory).
4. Add a `RenderPassConfig` for it in the app's pipeline build.

Each `RenderPass::Execute` records into the graphics queue's command list and **submits its own command list at the end** (see `RenderPass.cpp`).

### Submission model — read this before touching sync
Frames are pipelined `FRAMES_IN_FLIGHT` (2) deep. The only per-frame CPU wait is at the *top* of `Renderer::ExecutePipeline`, on the fence of the frame that last used this slot; `PresentFrame` records that fence and presents without blocking. Everything the CPU writes and the GPU reads within a frame is therefore slot-indexed: per-draw constants come from `FrameConstantAllocator` (a per-frame ring), longer-lived `ConstantBuffer`s are N-buffered internally via `s_FrameSlot`, and the command-allocator pool and transient descriptor heap size themselves from `FRAMES_IN_FLIGHT`. **New per-frame data must go through the ring or be N-buffered the same way — a single-buffered upload resource rewritten every frame is a race.** `GPUUploader` is asynchronous: copies go on the copy queue, acquire barriers on the graphics queue, and resources retire against a fence, so first-sighting a texture no longer stalls the frame.

### Resources & descriptors
- `ResourceManager` (singleton, `Resources/ResourceManager.h`) is the central factory/cache for buffers, textures, cubemaps, depth/render targets, shaders, PSOs, and root signatures. `AddShader`/`GetShader` register shaders by name — keep names consistent with what passes request.
- `DescriptorHeapManager` splits descriptors into a **persistent non-shader-visible staging heap** (one stable SRV per resource) and a **shader-visible transient heap** bump-allocated per frame; `ResourceManager::UpdateSRVDescriptors` copies from staging into a contiguous transient block each frame and returns its base handle.
- `PipelineStateCache` / `RootSignatureCache` hash-and-cache by content; `HashPSO` covers the full rasterizer/blend/depth-stencil state plus the root signature.

### Entity / scene model
- `Scene` (abstract, `Entity/Scene.h`) owns a `GameObjectContainer`, a `LightBuffer`, a `Camera`, and skybox cubemap + irradiance textures. Subclass it per scene (`ComplexDemoScene`, `ClusteredLightingScene`, `PhotogrammetryScene`, …) and select the active one in `DX12EngineDemoApp::Init`.
- `GameObject` is a transform + a bag of `Component`s. `CreateComponent<T>()`/`GetComponent<T>()` use RTTI; components include `RenderComponent`, `PhysicsComponent`, `ColliderComponent`, `AnimationComponent`. `GameObjectContainer` keys objects by string name.
- Models load via `ModelLoader`: `LoadObj`, `LoadGlb` (raw), and `LoadCookedModel(id)` (cooked output). Prefer relative model IDs under `res/Models/`, not absolute paths. `MaterialTemplate` lets a material carry its own PSO variant (blend policy, depth bias) that overrides the pass default.

### Hard limits & gotchas
- **Lighting is clustered** (`Rendering/Lighting/ClusteredLightGrid`): lights live in a runtime-sized `StructuredBuffer` and there is no light cap, but `MAX_LIGHT_INDICES` bounds the flat per-froxel index list (overflow silently drops lights; a `_DEBUG` print every 60 frames reports peak/dropped). The light list, froxel grid, index list and `ClusterData` are bound as **root SRVs/CBV in `space1`** — they consume no descriptors, so adding a consumer means adding root parameters, not heap entries. Shadow *slots* are still capped: `MAX_SHADOW_CASTING_SPOT_LIGHTS` / `..._POINT_LIGHTS` (4 each); lights past the cap get `ShadowMapIndex = -1` and shade unshadowed. See `docs/Clustered-Lighting-Guide.md`.
- The **transparent pass has two hand-kept-identical root signatures** — `TransparentRenderPass::CreatePSO` and `MaterialTemplate::BuildRootSignatureDesc` (`PassTarget::Transparent`). Nothing links them; a mismatch breaks only materials carrying a PSO variant. Edit both together.
- Shaders live in `src/DX12Engine/Shaders/` and `DemoScene/src/`; at runtime they're loaded from `res/Shaders/` next to the exe. CMake infers shader stage from filename: `*_VS.hlsl` → vertex, `*_PS.hlsl` → pixel. A `_CS` loader path exists in `ResourceManager`/`Shader`, but CMake warns-and-skips any file that is not `*VS*`/`*PS*`, so a compute shader needs that `foreach` fixed first; none ships today.
- Pipeline is HDR (`R16G16B16A16_FLOAT`) end-to-end; present applies exposure → ACES tone map → gamma, then optional FXAA. No bloom, no auto-exposure. Depth is **reverse-Z** (near → 1.0, far → 0.0, `GREATER` test).

## Conventions

- Engine code is under namespace `DX12Engine`; the demo under `DX12EngineDemo`.
- Both `CMakeLists.txt` files use `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so new `.cpp`/`.h`/`.hlsl` files are picked up on the next reconfigure — no manual source-list edits.
- Treat `res/` as runtime content. Large binary models are committed as plain files (no Git LFS); a fresh clone won't include anything left untracked locally.
