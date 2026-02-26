# DX12Engine

A modular, learning-oriented DirectX 12 rendering engine written in modern C++20. The repository includes a reusable engine static library, a sample client application path, shaders, and asset data for physically-based rendering (PBR), deferred lighting, shadows, and screen-space reflections (SSR).

## Highlights

- **Modern C++20 + CMake** build setup (`DX12Engine` is produced as a static library).  
- **DirectX 12 rendering framework** with command queues, descriptor heap management, render context/window abstraction, and pipeline state/root signature caches.  
- **Configurable multi-pass pipeline** with render passes for:
  - Shadow map
  - Cube shadow map
  - Geometry (G-Buffer)
  - Lighting
  - Screen-space reflection
  - UI (framework hook)
- **Material system** with basic and PBR materials.
- **Resource loading** for OBJ models (TinyObjLoader) and DDS/WIC textures (DirectXTex).
- **Simple rigid-body style physics** integration and collision handling primitives.
- **Sample scene wiring** in `ClientApplication` (models, textures, lights, camera, and render pipeline config).

## Repository Layout

```text
.
├── CMakeLists.txt
├── res/
│   ├── Materials/
│   └── Models/
└── src/
    ├── DX12Engine/
    │   ├── Entity/
    │   ├── IO/
    │   ├── Input/
    │   ├── Physics/
    │   ├── Rendering/
    │   ├── Resources/
    │   ├── Shaders/
    │   └── Utils/
    ├── ClientApplication.cpp
    ├── ClientApplication.h
    └── Main.cpp
```

## Requirements

> This codebase targets **Windows + DirectX 12**.

- Windows 10/11 SDK with Direct3D 12 support
- A GPU/driver stack supporting DirectX 12
- CMake 3.20+
- A C++20-capable compiler (MSVC recommended)
- Internet access during first configure/build (CMake fetches dependencies)

### Third-Party Dependencies (fetched by CMake)

- [DirectX-Headers](https://github.com/microsoft/DirectX-Headers)
- [DirectXTex](https://github.com/microsoft/DirectXTex)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader)

## Building

### 1) Configure

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### 2) Build

```bash
cmake --build build --config Release
```

This produces the static library target:

- `DX12Engine`

### 3) Runtime resources

The CMake build copies `res/` into the build directory and copies `.hlsl` shaders to `build/res/Shaders` via the `CopyEngineShaders` custom target.

## Running an Application

The current CMake file builds the engine as a static library. A sample app entry point exists in `src/Main.cpp`, but it is guarded by `#if ENABLE_TEST_PROJECT`.

You have two common options:

1. **Integrate DX12Engine into your own executable** and call `DX12Engine::Launcher::Launch(...)` with your `Application` subclass.
2. **Enable the test harness** by adding an executable target and compile definition in CMake, e.g.:

```cmake
add_executable(DX12EngineTest src/Main.cpp src/ClientApplication.cpp src/ClientApplication.h)
target_compile_definitions(DX12EngineTest PRIVATE ENABLE_TEST_PROJECT=1)
target_link_libraries(DX12EngineTest PRIVATE DX12Engine)
```

## Engine Architecture Overview

### Core loop

`DX12Engine::Launcher` creates a `RenderContext`, initializes your app, and runs a message/render loop that passes both per-frame delta time and elapsed time to `Application::Update`.

### Rendering

The renderer supports composition of render passes through `RenderPipelineConfig`, where each pass can consume typed input resources and prior pass outputs.

In the sample scene (`ClientApplication`), the configured pass order is:

1. ShadowMap
2. CubeShadowMap
3. Geometry
4. Lighting
5. ScreenSpaceReflection

### Resources

`ResourceManager` acts as a central factory/cache owner for:

- Shaders
- Buffers (vertex/index/constant)
- Textures/cubemaps/depth maps
- Pipeline states and root signatures

### Scene + ECS-style composition

Game objects can attach render and physics components. The sample scene creates multiple objects (`Cube`, `Ball`, `Floor`), assigns meshes/materials, and pushes them through physics + rendering updates each frame.

### Physics

The included `PhysicsEngine` updates component states, checks collisions, performs positional correction, and resolves impulses.

## Sample Controls

`InputHandler` maps commands to these defaults:

- `W/S/A/D`: move forward/back/left/right
- `E/Q`: move up/down
- `Right Mouse Button`: camera pan/look
- `Left Mouse Button`: interact hook

Mouse movement is consumed from window messages (`WM_MOUSEMOVE`) in the sample application.

## Shaders and Render Content

Shaders are stored in `src/DX12Engine/Shaders/*.hlsl` and include vertex/pixel programs for:

- Geometry pass
- Basic and PBR lighting
- Deferred lighting composite
- Shadow maps (2D + cube)
- Full-screen final render
- SSR pass

Assets provided in `res/` include:

- OBJ models (`cube`, `sphere`, `floor`, `cylinder`)
- PBR texture sets (albedo/normal/metallic/roughness/AO)
- Precomputed skybox cubemap + irradiance DDS files

## Notes and Limitations

- The project is Windows/DirectX12-specific and will not compile as-is on non-Windows platforms.
- The current root CMake config creates only a static library target; executable wiring is left to the consuming project or local test harness setup.
- Some systems (such as full UI rendering integration) are scaffolded in architecture but may be incomplete for production use.

## Development Tips

- When adding new passes, update `RenderPassType`, implement the pass class, and extend the renderer pass factory/creation logic.
- Keep shader names consistent with what `ResourceManager` registers.
- Treat `res/` as runtime content; ensure build/output copies stay in sync if you add assets.

## Contributing

1. Fork and create a feature branch.
2. Keep changes focused and include build/test notes.
3. Submit a pull request describing:
   - What changed
   - Why it changed
   - How to build/test

## License

No license file is currently included in this repository. If you intend to distribute or reuse this code, add an explicit license first.
