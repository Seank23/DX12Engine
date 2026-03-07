# DX12Engine

A modular, learning-oriented DirectX 12 rendering engine written in modern C++20. The repository includes a reusable engine static library, an embedded demo application (`DemoScene`), shaders, and asset data for physically-based rendering (PBR), deferred lighting, shadows, and screen-space reflections (SSR).

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
- **Embedded demo application** in `DemoScene/` showing how to build an executable against the engine library.

## Repository Layout

```text
.
|-- CMakeLists.txt
|-- DemoScene/
|   |-- CMakeLists.txt
|   `-- src/
|-- res/
|   |-- Materials/
|   `-- Models/
`-- src/
    |-- DX12Engine/
    |   |-- Entity/
    |   |-- IO/
    |   |-- Input/
    |   |-- Physics/
    |   |-- Rendering/
    |   |-- Resources/
    |   |-- Shaders/
    |   `-- Utils/
    |-- ClientApplication.cpp
    |-- ClientApplication.h
    `-- Main.cpp
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
cmake -S . -B build
```

### 2) Build

```bash
cmake --build build --config Release
```

This produces:

- `DX12Engine` static library
- `DemoScene` executable target

If you are generating with Visual Studio, the solution will be written to the build directory, for example:

- `build/DX12Engine.sln`

## DemoScene

`DemoScene` is an example executable that lives inside this repository and is added to the root build through `add_subdirectory(DemoScene)`.

### Build just the demo

```bash
cmake --build build --target DemoScene --config Debug
```

### Run the demo

After building, the executable is typically at:

- `build/DemoScene/Debug/DemoScene.exe`

When `DemoScene` is built, CMake also copies runtime content automatically:

- `res/` is copied to the build root: `build/res`
- `res/` is copied beside the executable: `build/DemoScene/<Config>/res`
- engine and demo `.hlsl` shaders are copied into both `build/res/Shaders` and `build/DemoScene/<Config>/res/Shaders`
- `dxcompiler.dll` and `dxil.dll` are copied beside `DemoScene.exe`

This means you can usually launch the built executable directly from Visual Studio or from the build output folder without manually copying assets.

## Runtime resources

The root build copies `res/` into the build directory and copies engine shaders to `build/res/Shaders`. `DemoScene` also performs its own post-build copy so assets and shaders are available next to the executable.

## Running your own application

The engine itself is built as a static library. To create your own executable, add a new target that links against `DX12Engine` and call `DX12Engine::Launcher::Launch(...)` with your `Application` subclass.

A legacy sample app entry point still exists in `src/Main.cpp`, but it is guarded by `#if ENABLE_TEST_PROJECT` and is not part of the normal repo build.

## Engine Architecture Overview

### Core loop

`DX12Engine::Launcher` creates a `RenderContext`, initializes your app, and runs a message/render loop that passes both per-frame delta time and elapsed time to `Application::Update`.

### Rendering

The renderer supports composition of render passes through `RenderPipelineConfig`, where each pass can consume typed input resources and prior pass outputs.

A typical deferred pipeline in this repository is:

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

Game objects can attach render and physics components. Sample scenes create objects such as `Cube`, `Ball`, and `Floor`, assign meshes/materials, and push them through rendering and optional physics updates each frame.

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
- The repository build currently includes the engine library and the `DemoScene` executable.
- Some systems (such as full UI rendering integration) are scaffolded in architecture but may be incomplete for production use.

## Development Tips

- When adding new passes, update `RenderPassType`, implement the pass class, and extend the renderer pass factory/creation logic.
- Keep shader names consistent with what `ResourceManager` registers.
- Treat `res/` as runtime content; ensure build/output copies stay in sync if you add assets.
- If you add another executable under the repo, prefer following the `DemoScene/` pattern for resource and shader copying.

## Contributing

1. Fork and create a feature branch.
2. Keep changes focused and include build/test notes.
3. Submit a pull request describing:
   - What changed
   - Why it changed
   - How to build/test

## License

No license file is currently included in this repository. If you intend to distribute or reuse this code, add an explicit license first.
