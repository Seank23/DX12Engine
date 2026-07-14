# C++ Style Guide

A portable, self-contained coding-style reference distilled from the DX12Engine codebase. Drop this file into any C++ project (and reference it from `CLAUDE.md`) to keep a consistent house style. Rules are stated as directives with real examples; graphics-specific idioms are isolated in the last section so the rest generalizes to any modern C++ project.

Target: **C++20**, MSVC-first.

---

## 1. Formatting

- **Indent with tabs**, one tab per level. Do not mix tabs and spaces within a file.
- **Allman braces**: opening brace on its own line for functions, classes, namespaces, and control blocks.
- **Namespace bodies are indented** one level (everything lives under one project namespace — see §3).
- **Single-statement blocks omit braces** and put the statement on the next line, indented:
  ```cpp
  if (!listener)
      return;

  for (const auto& component : m_Components)
      component->Init();
  ```
  Use braces when the body spans multiple lines or contains nested control flow.
- One blank line between functions; blank lines inside functions to separate logical groups. No blank line after an opening brace or before a closing brace.
- No hard column limit — let related arguments stay on one line even when long. When a signature *is* wrapped, put each parameter on its own line, indented one level:
  ```cpp
  void UpdateConstantBufferData(
      ResolvedPrimitiveBinding& binding,
      DirectX::XMMATRIX modelMatrix,
      DirectX::XMMATRIX viewMatrix,
      DirectX::XMFLOAT3 cameraPosition);
  ```
- Pointer/reference binds to the type: `Type* ptr`, `Type& ref` (not `Type *ptr`).

---

## 2. Naming

| Kind | Convention | Example |
|------|-----------|---------|
| Class / struct / enum type | `PascalCase` | `RenderComponent`, `ResolvedPrimitiveBinding` |
| Methods / free functions | `PascalCase` | `Update`, `GetModelMatrix`, `RebuildResolvedPrimitiveBindings` |
| Enum values | `PascalCase` | `ComponentType::Render`, `RenderPassType::Geometry` |
| Private/protected members | `m_PascalCase` | `m_Position`, `m_ResolvedPrimitiveBindings` |
| Public struct fields (POD/data) | `PascalCase`, **no** `m_` | `ModelMatrix`, `CBVAddress`, `NodeIndex` |
| Parameters / locals | `camelCase` | `modelMatrix`, `listenerIt`, `nodeCount` |
| Compile-time constants / macros | `SCREAMING_SNAKE_CASE` | `MAX_LIGHTS`, `SHADOW_MAP_SIZE` |
| Interfaces (pure-virtual) | `I` prefix | `IColliderListener`, `IUIBackend` |

- Getters are `GetX()`; setters are `SetX(...)`; boolean queries may read as `GetIsReady()` / `ShouldSkipUpdate()`.
- Files are `PascalCase.h` / `PascalCase.cpp`, named after the primary type they contain.
- Prefer descriptive names over abbreviations; short loop indices (`i`, `ni`) are fine in tight loops.

---

## 3. File organization

- Every header starts with `#pragma once` (never include guards).
- **Everything is wrapped in a single project namespace** (here `DX12Engine`; the app layer uses its own, e.g. `DX12EngineDemo`). Contents are indented one level inside it.
- **Include order** in a `.cpp`: its own header first, then project headers (relative paths), then third-party/system headers:
  ```cpp
  #include "RenderComponent.h"          // own header
  #include "../Resources/ResourceManager.h"
  #include "GameObject.h"
  #include <memory>                      // system/std last
  ```
- Prefer **forward declarations** in headers over `#include` to cut compile dependencies; include the full definition in the `.cpp`:
  ```cpp
  namespace DX12Engine
  {
      class MeshPrimitive;   // forward-declared, only pointers/refs used in this header
      class MaterialAsset;
  ```
- Keep headers lean: declarations + trivial inline accessors only. Non-trivial logic lives in the `.cpp`.

---

## 4. `struct` vs `class`

Choose deliberately — the distinction carries meaning here:

- **`struct`** = passive data aggregate. Public fields in `PascalCase`, **default member initializers**, little or no behavior. Used for config, GPU-facing data, and resolved/cached records:
  ```cpp
  struct ResolvedPrimitiveBinding
  {
      MeshPrimitive* Primitive = nullptr;
      int NodeIndex = -1;
      std::unique_ptr<ConstantBuffer> PrimitiveConstantBuffer;
      D3D12_GPU_VIRTUAL_ADDRESS CBVAddress = 0;
      bool HasValidPrevUnjitteredMVP = false;
  };
  ```
- **`class`** = behavior + invariants. Private `m_`-prefixed state, public method surface. Used for anything with logic or ownership.

---

## 5. Classes

- **Access sections in order:** `public:` → `protected:` → `private:`. Constructors/destructor first under `public:`, then methods, with data members last (usually `private:`/`protected:`).
- **Trivial accessors are defined inline** in the header; everything else is declared in the header and defined in the `.cpp`:
  ```cpp
  D3D12_RESOURCE_STATES GetUsageState() const { return m_UsageState; }
  void SetUsageState(D3D12_RESOURCE_STATES usageState) { m_UsageState = usageState; }
  ```
- Mark const-correct methods `const`. Mark overrides with **both** `virtual` and `override`:
  ```cpp
  virtual void Init() override;
  virtual void Update(float ts, float elapsed) override;
  ```
- Abstract base classes declare pure-virtual interface (`= 0`) and a `virtual ~Base() = default;`.
- **Constructor initializer lists**: one member per line when there are several, aligned under the `:`:
  ```cpp
  GameObject::GameObject()
      : m_Position({ 0.0f, 0.0f, 0.0f }),
      m_Scale({ 1.0f, 1.0f, 1.0f }),
      m_Rotation(DirectX::XMQuaternionIdentity())
  {
  }
  ```
  Short lists may stay on one line. Base-class init comes first. `std::move` sink parameters into members.
- Delete copy where ownership makes copying wrong: `Type(const Type&) = delete;`.
- Use `friend` sparingly to grant a collaborating system (e.g. a renderer) access to a component's internals rather than widening the public API.

---

## 6. Memory & ownership

- `std::unique_ptr<T>` for **exclusive ownership** (the default for owned resources and buffers).
- `std::shared_ptr<T>` for **shared assets** genuinely referenced by multiple owners (models, textures, scene objects).
- **Raw pointers/references are non-owning** observers only — never `delete` through them. Getters that expose owned objects return the raw pointer via `.get()`:
  ```cpp
  ModelInstance* GetAsset() const { return m_Asset.get(); }              // non-owning view
  std::shared_ptr<ModelInstance> GetAssetShared() const { return m_Asset; } // shared ownership
  ```
- Construct with `std::make_unique` / `std::make_shared`. `std::move` into members and containers.
- Prefer range-based `for` with `const auto&` (or explicit type for clarity); take a mutable `auto&`/`T&` when mutating in place.

---

## 7. Error handling

- Use **exceptions** for unrecoverable failures, funneled through small static helpers rather than scattered `if (FAILED(...))`:
  ```cpp
  EngineUtils::ThrowIfFailed(hr);   // throws std::runtime_error(FormatHRESULT(hr)) on failure
  EngineUtils::Assert(size <= m_BufferSize);
  ```
- Validate preconditions with **early returns** (guard clauses) at the top of a function; keep the happy path un-indented:
  ```cpp
  if (!m_Asset)
      return;
  ModelAsset* modelAsset = m_Asset->GetModelAsset();
  if (!modelAsset)
      return;
  ```
- Group small cross-cutting helpers as `static` methods on a utility class (`EngineUtils`) rather than free functions floating in the namespace.

---

## 8. Idioms & patterns

- **Cast with C++ casts** — `static_cast`, `reinterpret_cast`. (C-style casts like `(float)x` appear in hot render code but are not the preferred style for new code.)
- **Prefer `enum class`** for type-safe enumerations; plain `enum` only for values used as bare integers/flags.
- **Singletons** expose a static `GetInstance()` with a private constructor and deleted copy, plus explicit `Init()`/`Shutdown()` for lifetime control (`ResourceManager`).
- **Fluent builders** return `*this` for chaining and produce an immutable config object:
  ```cpp
  PipelineBuilder& AddPass(const RenderPassConfig& pass) { m_Passes.push_back(pass); return *this; }
  // usage: builder.AddPass(a).AddPass(b).AddPassIf(cond, c).Build();
  ```
- **Data-driven configuration:** describe *what* with plain `struct` configs (`RenderPassConfig`, `RendererOptions`) and let a system interpret them, rather than hard-coding behavior.
- **Component/entity composition:** entities own a `std::vector<std::unique_ptr<Component>>`; add/query via templates:
  ```cpp
  template<typename T, typename... Args>
  T* CreateComponent(Args&&... args);
  template<typename T> T* GetComponent();   // dynamic_cast lookup
  ```
- **Factories** centralize construction (`ResourceManager::Create*`, a pass `CreateRenderPass(type)` switch) so callers never `new` directly.

---

## 9. Comments

- Use `//` line comments to explain **why**, not restate the code. Reserve them for non-obvious rationale, invariants, and gotchas:
  ```cpp
  // Persistent descriptor: set once at resource creation, never overwritten.
  // Lives in the non-shader-visible staging heap; used as CopyDescriptorsSimple source.
  DescriptorHeapHandle* GetPersistentDescriptor() const { return m_PersistentDescriptor.get(); }
  ```
- A short comment introducing a non-trivial loop or branch is welcome ("Fallback: if no nodes reference any mesh…").
- Trailing field comments are fine to annotate parallel/array data (e.g. G-buffer target formats).
- No decorative banners, no commented-out dead code left in commits.

---

## 10. Graphics / DirectX-specific conventions

*(Skip this section in non-graphics projects.)*

- Wrap COM objects in `Microsoft::WRL::ComPtr<T>`; store raw `ID3D12*` only for non-owning references.
- Every fallible D3D call goes through `EngineUtils::ThrowIfFailed(...)`.
- Resources derive from a common `GPUResource` base tracking `ID3D12Resource*`, current `D3D12_RESOURCE_STATES`, GPU address, and descriptor handles; state transitions go through `SetUsageState` alongside the barrier so CPU-side tracking stays in sync.
- Render passes derive from a `RenderPass` base and override `Init` / `Execute` / `CreatePSO` / `OnResize`; the derived `Execute` calls `RenderPass::Execute()` first.
- Prefer batching barriers into a single `ResourceBarrier` array over many individual calls.

---

**When in doubt, match the surrounding file.** Consistency within a file beats global uniformity; if a file already deviates, follow its local convention rather than mixing two styles in one place.
