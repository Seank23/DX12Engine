# DX12Engine

DX12Engine is a Windows-only DirectX 12 rendering engine written in C++20. The current repository state is an engine/static-library codebase rather than a turnkey game or demo executable: CMake builds the `DX12Engine` static library, copies runtime content into the build tree, and leaves application wiring to a downstream executable.

The engine contains a Direct3D 12 render context, command queues, descriptor heaps, GPU upload helpers, render passes, scene/object abstractions, resource loading, PBR-oriented materials, camera input helpers, and a small physics layer.

## Current Status

- **Build target:** `DX12Engine` static library.
- **Platform:** Windows with DirectX 12. The source includes Win32 and D3D12 headers/libraries and is not portable to Linux/macOS as-is.
- **Application entry point:** `src/Main.cpp` and `src/ClientApplication.*` are guarded by `ENABLE_TEST_PROJECT` and are not wired into the root CMake target as a runnable executable.
- **Primary integration model:** create your own executable, link against `DX12Engine`, implement `DX12Engine::Application`, and drive rendering through `DX12Engine::Launcher`/`Renderer`.
- **Runtime content:** CMake copies `res/` and engine shaders into the build output under `res/`.

## Feature Overview

### Rendering

- Direct3D 12 render context and Win32 window wrapper.
- Graphics command queue management and fence synchronization.
- Render-pass based pipeline configuration.
- Built-in render pass types:
  - `ShadowMap`
  - `CubeShadowMap`
  - `Geometry`
  - `Lighting`
  - `ScreenSpaceReflection`
  - `UI`
- Deferred/PBR-oriented shader set with G-Buffer outputs, lighting composite, final fullscreen presentation, and SSR.
- Pipeline state and root signature caches/builders.
- Descriptor heap management for staging, SRV/CBV, RTV/DSV, and render-pass descriptors.

### Scene and Entity Model

- `DX12Engine::Scene` owns scene objects, a light buffer, a camera, and skybox cubemap/irradiance textures.
- `DX12Engine::GameObject` supports transform operations, mesh assignment, and component creation/lookup.
- `GameObjectContainer` stores named objects and can collect components across all objects.
- Renderer scene binding is performed with `Renderer::SetCurrentScene(Scene*)`; `Renderer::ExecutePipeline(...)` pulls render components, camera, lights, and texture readiness from the current scene before executing passes.

### Resources and Assets

- `ResourceManager` creates and owns GPU-facing resources such as vertex/index/constant buffers, textures, cubemaps, render targets, depth maps, root signatures, and pipeline states.
- Built-in shader names are registered during `ResourceManager` construction and resolved from `res/Shaders/` at runtime.
- `ModelLoader` loads OBJ geometry through TinyObjLoader and computes tangents for normal-mapped materials.
- `TextureLoader` loads DDS cubemaps and WIC textures, and can load material directories containing `albedo.png`, `normal.png`, `metallic.png`, `roughness.png`, and `ao.png`.

### Materials, Lighting, and Physics

- Basic and PBR material classes are included.
- `LightBuffer` supports directional, point, and spot light data for render passes.
- `PhysicsEngine` updates physics components, checks collisions, applies positional correction, and resolves collision impulses.
- Collision mesh types include sphere, box, and plane-style primitives.

### Input

- `InputHandler` maps movement commands to keyboard/mouse defaults:
  - `W/S/A/D`: forward/back/left/right
  - `E/Q`: up/down
  - Right mouse button: pan/look
  - Left mouse button: interact hook
- `InputHandler` is currently an abstract base because `HandleMouseWheel(HWND, WPARAM)` is pure virtual; applications should derive from it or provide their own input adapter.
- `Application::HandleWindowEvent` receives `HWND`, `UINT`, `WPARAM`, and `LPARAM`, so applications can handle keyboard, mouse, wheel, resize, and other Win32 messages.

## Repository Layout

```text
.
├── CMakeLists.txt                  # Root CMake configuration; builds DX12Engine static library
├── README.md
├── res/
│   ├── Materials/                  # PBR texture sets and skybox cubemaps/irradiance maps
│   └── Models/                     # OBJ/MTL model assets
└── src/
    ├── Main.cpp                    # Optional/guarded sample entry point
    ├── ClientApplication.*         # Optional/guarded sample client scene setup
    └── DX12Engine/
        ├── Entity/                 # Scene, GameObject, components
        ├── IO/                     # OBJ and texture loading
        ├── Input/                  # Camera/input command handling
        ├── Physics/                # Physics components, collision, solver
        ├── Rendering/              # Render context, renderer, passes, queues, heaps, builders
        ├── Resources/              # GPU resources, shaders, textures, materials, lights
        ├── Shaders/                # HLSL shader sources
        └── Utils/                  # Utility helpers/constants
```

## Requirements

This project is intended to be configured and built on Windows.

- Windows 10/11
- Windows SDK with Direct3D 12 headers and libraries
- DirectX 12-capable GPU and driver
- CMake 3.20+
- C++20-capable compiler, with MSVC recommended
- Git/network access for CMake `FetchContent` dependency downloads

CMake fetches these dependencies:

- [microsoft/DirectX-Headers](https://github.com/microsoft/DirectX-Headers) from `main`
- [microsoft/DirectXTex](https://github.com/microsoft/DirectXTex) from `main`
- [tinyobjloader/tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) from `release`

The target also links against Windows/DirectX libraries including `d3d12`, `dxgi`, `dxguid`, `D3DCompiler`, and `dxcompiler`.

## Building the Library

From the repository root:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The root `CMakeLists.txt` currently:

1. Requires CMake 3.20 and C++20.
2. Fetches DirectX-Headers, DirectXTex, and tinyobjloader.
3. Recursively includes `src/*.cpp`, `src/*.h`, and `src/*.hlsl` in `ENGINE_SOURCES`.
4. Builds `DX12Engine` as a static library.
5. Copies `res/` into the CMake binary directory.
6. Copies shader files from `src/DX12Engine/Shaders/*.hlsl` to `${CMAKE_BINARY_DIR}/res/Shaders` through the `CopyEngineShaders` target.

> Note: because this code links DirectX/Win32 libraries, configure/build validation from non-Windows CI or containers is expected to fail unless a compatible Windows toolchain and SDK are available.

## Using DX12Engine from an Application

A consuming application is expected to provide an executable target that links to `DX12Engine` and implements the `DX12Engine::Application` interface.

At minimum, your application subclass must implement:

```cpp
void Init(std::shared_ptr<DX12Engine::RenderContext> renderContext,
          DirectX::XMFLOAT2 windowSize) override;

void Update(float ts, float elapsed) override;

void HandleWindowEvent(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
```

A typical application flow is:

1. Create an `Application` subclass.
2. Launch it with `DX12Engine::Launcher::Launch(&app, windowSize, windowTitle)`.
3. In `Init`, create a `Renderer`, create/initialize a `Scene`, call `renderer.SetCurrentScene(...)`, and build a `RenderPipeline`.
4. In `Update`, update input, scene/game objects, light buffers, and physics, then call `renderer.ExecutePipeline(pipeline)`.
5. In `HandleWindowEvent`, route Win32 messages to your input and resize handling.

## Configuring a Render Pipeline

Render pipelines are described with `RenderPipelineConfig`, which contains an ordered list of `RenderPassConfig` entries. Each pass declares its type and can reference prior pass outputs with `InputResourceType` values such as:

- `RenderTargets_ShadowMap`
- `RenderTargets_CubeShadowMap`
- `RenderTargets_Geometry`
- `RenderTargets_Lighting`
- `ExternalTextures`
- `VertexShader`
- `PixelShader`

For a deferred scene with shadows and SSR, the intended pass order is generally:

1. `ShadowMap`
2. `CubeShadowMap`
3. `Geometry`
4. `Lighting`
5. `ScreenSpaceReflection`

If the final pass exposes a `Composite` render target, `Renderer::ExecutePipeline(...)` presents that target to the swap chain.

## Runtime Content Conventions

The engine uses relative runtime paths through `ResourceManager`:

- Materials: `res/Materials/...`
- Models: `res/Models/...`
- Shaders: `res/Shaders/...`

Keep this layout available next to the executable/build output. The provided CMake logic already copies `res/` and shader files into the build tree for local builds.

## Shader Inventory

Current HLSL sources include:

- `BasicLighting_VS.hlsl`
- `BasicLighting_PS.hlsl`
- `PBRLighting_VS.hlsl`
- `PBRLighting_PS.hlsl`
- `PBRLightingDeferred_PS.hlsl`
- `Geometry_VS.hlsl`
- `Geometry_PS.hlsl`
- `ShadowMap_VS.hlsl`
- `ShadowCubeMap_VS.hlsl`
- `ShadowCubeMap_PS.hlsl`
- `RenderTriangle_VS.hlsl`
- `FinalRender_PS.hlsl`
- `SSRPass_PS.hlsl`

When adding a new built-in shader, either register it with `ResourceManager::AddShader(...)` or add it to the default shader map in `ResourceManager`.

## Known Limitations

- The repository does not currently define a runnable executable target in the root CMake file.
- `src/ClientApplication.*` is guarded sample/reference code and may need synchronization with the latest abstract interfaces before being used as a test executable.
- The root CMake configuration tracks dependency branches (`main`/`release`) rather than pinned commits, so dependency behavior can change over time.
- Resource paths are relative and assume the working directory contains the copied `res/` tree.
- No automated test suite is currently included.
- No license file is currently present; add one before distributing or reusing the code outside its current context.

## Development Notes

- Add new render passes by extending `RenderPassType`, implementing a `RenderPass` subclass, and updating the `Renderer::CreateRenderPass` factory.
- If a pass consumes outputs from earlier passes, add the appropriate `InputResourceType` handling in `Renderer::CreateRenderPipeline`.
- Keep shader filenames and registered shader names synchronized.
- Update resource copying rules if new runtime asset directories are added.
- Prefer deriving application-specific input from `InputHandler` rather than editing engine input defaults directly.
