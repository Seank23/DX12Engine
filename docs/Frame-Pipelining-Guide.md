# Tier 2a — Frame pipelining: Implementation Guide

A step-by-step guide for implementing items **4, 5 and 7** of the Tier 2 roadmap in
`docs/Rendering-Architecture.md` yourself: real CPU/GPU frame overlap, a per-frame ring constant
allocator, and non-blocking uploads. It assumes you know C++20 and D3D12 fundamentals (fences,
barriers, upload heaps, command allocators), so it concentrates on **this engine's specific
lifetime assumptions** — which buffers are currently single-copy, which reset points are currently
load-bearing, and what silently serialises the queue if you miss it.

It does not hand you finished `.cpp` bodies. The ring allocator, the fence gate and the async upload
path are given as declarations and pseudocode; you write the logic.

Item 6 (slim the G-buffer) is deliberately **not** here — it is an independent change with its own
guide, `docs/GBuffer-Slimming-Guide.md`. The two can be done in either order.

## What we're building

1. **A per-frame ring constant allocator** (`FrameConstantAllocator`) that replaces the ~one
   committed 64 KB-aligned upload resource *per primitive binding* with `FRAMES_IN_FLIGHT` large
   sub-allocated buffers.
2. **An N-buffered `ConstantBuffer`** so every fixed-count dynamic CB (screen data, light buffer,
   SSR/TAA temporal, CSM, post-processing, material) carries one copy per in-flight frame — with
   zero changes at the ~15 call sites that bind them.
3. **Removal of the end-of-frame `WaitForFenceCPUBlocking`**, replaced by a fence gate at the *top*
   of the next frame that only waits for frame `N − FRAMES_IN_FLIGHT`.
4. **Non-blocking uploads**: the copy→graphics hand-off becomes a GPU-side `Wait` instead of two CPU
   stalls, with upload resources retired by fence.

These ship together because they are not separable. The full-frame stall is what makes single-copy
dynamic buffers safe (§3.2 of the architecture doc says so explicitly). Remove the stall without
(1) and (2) and you get *silent* corruption — the CPU rewriting matrices the GPU is mid-read on.
Do (1) and (2) without (3) and you have spent the memory for no throughput.

## Why this one is different

Every rendering change you have made so far has been *spatial* — a pass, a target, a shader. This
one is **temporal**, and it retires an invariant the whole engine has been leaning on: *at any
moment, exactly one frame's GPU work exists, and it is finished before the CPU touches anything
again.* Three concerns move to the front:

- **Lifetime, not correctness of values.** After this change the question for every byte the CPU
  writes is "could the GPU still be reading the previous contents?" A wrong answer does not throw or
  assert — it produces a one-frame-stale matrix, a flickering material, or geometry lagging its
  shadow. Debug-layer-clean and wrong at the same time.
- **Implicit serialisation points.** Removing the *explicit* stall does not give you overlap if an
  *implicit* one remains. The command-allocator pool (§4) is the big one: 8 allocators against 10
  submissions per frame means `ResetCommandAllocatorAndList` CPU-blocks partway through every frame,
  and your profiler will show the same frame time as before with no obvious cause.
- **Reset points become fence-gated.** Anything that says "start of frame, set cursor to 0" —
  the transient descriptor heap, the Rml transient vertex/index/constant offsets, the ring — is now
  resetting memory a previous frame may still own. Each such reset needs either N slots or a fence.

> **Design decisions (settled)**
>
> - `FRAMES_IN_FLIGHT` moves from `RenderPassDescriptorHeap.h` to `Utils/Constants.h` and becomes
>   the single knob for buffering depth (§4). Everything sizes off it: swap-chain buffers, RTV heap,
>   allocator pool, CB slots, ring slots.
> - **Two mechanisms, not one.** Fixed-count CBs get N-buffered *inside* `ConstantBuffer` (§5);
>   per-object CBs move to the ring (§6). Rationale in §5a and §6a.
> - `ResourceManager` owns the ring and the current frame slot, because `RenderComponent` already
>   reaches it via `GetInstance()` and holds no `RenderContext` (§6c).
> - Upload correctness after (4) rests on a **GPU-side cross-queue wait**, not a CPU wait — the
>   pattern `RmlRenderInterfaceDX12.cpp:764` already uses (§8).

> **Still-open micro-forks — my defaults, veto cheaply**
>
> - Ring size: **4 MB per frame slot** (`RenderComponentData` is 464 B → 512 B aligned, so ~8000
>   bindings/frame). Overflow throws with a message, matching `RenderPassDescriptorHeap`.
> - Allocator pool: **`FRAMES_IN_FLIGHT * 16`** (32 at N=2). Cheap objects; over-provision.
> - `GPUResource::GetGPUAddress()` becomes **`virtual`** rather than shadowed in `ConstantBuffer`.
> - Ship at **N = 2**, flip to 3 as the last slice and keep whichever measures better (§9).

> **The invariant contract for this task**
>
> Any GPU-visible byte the CPU writes each frame must exist in `FRAMES_IN_FLIGHT` copies, or be
> written only after a fence proves the GPU is done with it. No CPU wait may remain in the
> steady-state frame path except the one frame gate. Audited in §12.

---

## 1. Scope

In scope: the submission model, dynamic constant-buffer storage, the upload path, and the
constants/pools that size them. The observable target is **CPU frame time roughly halved** at a
fixed GPU load, no first-sighting texture hitch, and a clean D3D12 debug layer.

**Untouched — if your diff reaches these, you have overreached:**

- Every `.hlsl` and `.hlsli`. Not one shader changes.
- The pass graph: `RenderPipelineConfig.h`, `PipelineBuilder`, `Renderer::CreateRenderPipeline`,
  and every `RenderPass` subclass's `Init`/`Execute`/`CreatePSO`/`GetRenderTarget`.
- `DemoScene/` — the demo's pipeline build and scenes are unchanged.
- Descriptor allocation strategy. `DescriptorHeapManager`, `StagingDescriptorHeap` and
  `RenderPassDescriptorHeap` keep their current design; the transient heap is *already* N-buffered
  and needs no edit (see the §13 caveat on why its capacity now matters more).
- Barrier placement and resource states.
- Anything on the Tier 3 list — no compute, no clustering, no multi-threaded recording.

## 2. Files you'll touch

| File | Change |
|---|---|
| `src/DX12Engine/Utils/Constants.h` | Add `FRAMES_IN_FLIGHT`, `CONSTANT_RING_BYTES_PER_FRAME`, `COMMAND_ALLOCATOR_POOL_SIZE` |
| `src/DX12Engine/Rendering/Heaps/RenderPassDescriptorHeap.h` | Delete its local `FRAMES_IN_FLIGHT` (line 8); include `Constants.h` |
| `src/DX12Engine/Rendering/Queues/CommandQueue.cpp` | `kCommandAllocatorPoolSize` (line 11) sized off the new constant |
| `src/DX12Engine/Rendering/RenderWindow.h` / `.cpp` | `m_RenderTargets[2]` → `[FRAMES_IN_FLIGHT]`; `BufferCount`, RTV heap `NumDescriptors` and both `for (i < 2)` loops parameterised |
| `src/DX12Engine/Resources/GPUResource.h` | `GetGPUAddress()` → `virtual` |
| `src/DX12Engine/Rendering/Buffers/ConstantBuffer.h` / `.cpp` | N sub-ranges in one resource; `Update` writes current slot; `UpdateAllFrames`; static `SetFrameSlot`; override `GetGPUAddress` |
| `src/DX12Engine/Rendering/Buffers/FrameConstantAllocator.h` / `.cpp` | **New** — `FRAMES_IN_FLIGHT` persistently-mapped upload buffers, bump-allocated, reset per slot |
| `src/DX12Engine/Resources/ResourceManager.h` / `.cpp` | `CreateConstantBuffer` allocates `N × alignedSize`; own the ring; add `BeginFrame(slot)` and `AllocateFrameConstants(data, size)` |
| `src/DX12Engine/Entity/RenderComponent.h` / `.cpp` | Drop `PrimitiveConstantBuffer` from `ResolvedPrimitiveBinding`; `Init` no longer creates CBs; `UpdateConstantBufferData` sets `CBVAddress` from the ring |
| `src/DX12Engine/Rendering/Renderer.h` / `.cpp` | `m_FrameFenceValues[]`; fence gate at top of `ExecutePipeline`; remove the stall in `PresentFrame` (line 606); `m_OptionsDirty` |
| `src/DX12Engine/Rendering/GPUUploader.h` / `.cpp` | `ExecuteUpload` → GPU-side cross-queue wait, no CPU wait; fence-retired release lists |
| `src/DX12Engine/Resources/Materials/Material.cpp` | `UpdateConstantBufferData` uses `UpdateAllFrames` |
| `src/DX12Engine/UI/RuntimeBackend/RmlRenderInterfaceDX12.h` / `.cpp` | N-buffer the transient vertex/index/constant regions (§13) |
| `docs/Rendering-Architecture.md` | §3.2, §3.3, §10.3, §14, §16 updated at close-out |
| `CMakeLists.txt` | **No edit** — both lists use `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so the new pair is picked up on the next build's reconfigure |

Conspicuously absent and deliberately so: `Launcher.h` (the frame loop needs no change — the gate
lives inside `ExecutePipeline`) and `Application.h` (the app contract is unchanged).

## 3. Suggested build order

Six slices. Each builds, runs, and is visually identical to the last except where noted — which is
the point: you are changing *when* memory is safe, not what it contains, until slice 4.

1. **Constants and pools.** Hoist `FRAMES_IN_FLIGHT`, parameterise `RenderWindow`, grow the
   allocator pool. Build and run — pixel-identical, still stalling. Proves the parameterisation is
   complete before anything depends on it.
2. **N-buffered `ConstantBuffer`.** Build and run — still stalling, so still correct by the old
   invariant; identical image. Proves N-buffering did not break address binding, which is the one
   thing that would show up as garbage geometry.
3. **The ring.** Per-object CBs move off committed resources. Build and run — identical image;
   process VRAM and allocation count drop measurably. Still stalling.
4. **Remove the stall.** One axis of risk, on top of three de-risked slices. Build and run —
   *now* the image can break, and if it does you know which change caused it.
5. **Async uploads.** Build and run — the hitch on first sighting of a texture disappears.
6. **Flip `FRAMES_IN_FLIGHT` to 3.** Measure; keep the better number.

Slices 1–3 are safe to commit independently. Slice 4 is the one to bisect to if anything goes wrong
later.

---

## 4. Slice 1 — one knob, correctly sized pools

### 4a. Why this shape

`FRAMES_IN_FLIGHT` currently lives at `RenderPassDescriptorHeap.h:8` — inside the one subsystem that
already honoured it. Four other things need the same number (swap-chain buffers, RTV descriptors,
CB slots, ring slots) and one thing needs a multiple of it (the allocator pool). Leaving it where it
is means either a bad include (`RenderWindow.h` pulling in a descriptor heap header) or a second
copy that drifts. `Utils/Constants.h` is already the home for `SHADOW_MAP_SIZE`, `MAX_CSM_CASCADES`
and friends, and is included transitively almost everywhere via `RenderPassData.h`.

That file currently uses `#define`. Follow it in spirit but not in mechanism — a typed
`static constexpr` is what the existing `FRAMES_IN_FLIGHT` already is, and it needs to be usable as
an array bound.

### 4b. Structure

In `Utils/Constants.h`, at global scope alongside the existing macros:

```cpp
static constexpr unsigned int FRAMES_IN_FLIGHT = 2;

// Per-frame ring capacity for per-object constants. RenderComponentData is 464 B → 512 B
// after 256-byte CBV alignment, so this covers ~8000 primitive bindings per frame.
static constexpr unsigned int CONSTANT_RING_BYTES_PER_FRAME = 4 * 1024 * 1024;

// One allocator per command-list submission per in-flight frame, plus slack for uploads.
// The demo submits ~10 lists/frame (9 passes + present).
static constexpr unsigned int COMMAND_ALLOCATOR_POOL_SIZE = FRAMES_IN_FLIGHT * 16;
```

Then delete line 8 of `RenderPassDescriptorHeap.h` and add `#include "../../Utils/Constants.h"`.
Note the namespace change: the old constant was inside `namespace DX12Engine`, the new one is
global. Every current use is unqualified inside the namespace, so it still resolves — but check
`ImGuiDebugBackend.cpp:304` builds, since that is the one use outside `Rendering/`.

### 4c. The allocator-pool trap

This is the single most important line in slice 1. `CommandQueue.cpp:11` declares
`kCommandAllocatorPoolSize = 8`. `ResetCommandAllocatorAndList` (line 106) scans the pool for a slot
whose fence has completed and, finding none, **CPU-blocks** on the next one (lines 124–132).

The demo submits ten command lists per frame. With 8 allocators and the stall in place this never
bites, because the GPU is always idle by the time you come round again. Remove the stall and slot 0
— retired at submission 1 of the *current* frame — is still in flight when submission 9 asks for it.
You get a stall, in the middle of the frame, that looks like nothing and costs everything.

Set the pool from `COMMAND_ALLOCATOR_POOL_SIZE`. Command allocators are cheap; 32 is not extravagant.

### 4d. Parameterising `RenderWindow`

Mechanical but must be *complete*, or slice 6 silently does nothing. Five places in
`RenderWindow.cpp` / `.h` hard-code 2:

| Location | Current | Change to |
|---|---|---|
| `RenderWindow.h:54` | `ComPtr<ID3D12Resource> m_RenderTargets[2]` | `[FRAMES_IN_FLIGHT]` |
| `RenderWindow.cpp:65` | `swapChainDesc.BufferCount = 2` | `FRAMES_IN_FLIGHT` |
| `RenderWindow.cpp:85` | `rtvHeapDesc.NumDescriptors = 2` | `FRAMES_IN_FLIGHT` |
| `RenderWindow.cpp:93` | `for (UINT i = 0; i < 2; i++)` | `i < FRAMES_IN_FLIGHT` |
| `RenderWindow.cpp:159` | `ResizeBuffers(0, ...)` | leave as-is — `0` means "keep current count", which is correct |

At N = 2 this is a no-op, which is exactly what makes it a good first slice: build, run, confirm
nothing changed.

### 4e. Checkpoint

```
cmake --build ../DX12Engine_build --target DemoScene --config Debug
../DX12Engine_build/DemoScene/Debug/DemoScene.exe
```

Identical image, identical frame time, debug layer silent.

---

## 5. Slice 2 — N-buffered `ConstantBuffer`

### 5a. Why this shape, and why not the ring for everything

There are ten `CreateConstantBuffer` call sites (grep confirms: `RenderComponent.cpp:29`,
`LightBuffer.cpp:10`, `RenderContext.cpp:31`, `Renderer.cpp:133`,
`CascadedShadowMapRenderPass.cpp:43`, `LightingRenderPass.cpp:46`, `SSRRenderPass.cpp:48`,
`TAARenderPass.cpp:67`, `Material.cpp:8` and `:13`). Nine of them create exactly one buffer with a
fixed lifetime; only `RenderComponent.cpp:29` creates one *per primitive binding*.

**Option A — route everything through the ring.** Each pass calls `Allocate(&data, size)` in
`Execute` and binds the returned address. Uniform, and `ConstantBuffer` could disappear. But it
edits every pass's `Execute`, and the write point (`Update`) and the bind point are currently far
apart — `LightBuffer::Update()` is called from `DX12EngineDemoApp::Update`, bound in
`LightingRenderPass::Execute`. You would have to move or duplicate that logic in seven places.

**Option B — N-buffer inside `ConstantBuffer` (recommended).** One resource of `N × alignedSize`,
`Update` writes the current slot, `GetGPUAddress` returns the current slot's base. Every existing
call site — `m_TemporalCB->GetGPUAddress()`, `GetScreenDataBuffer().GetGPUAddress()`,
`lightBuffer->GetCBVAddress()` — keeps working untouched.

Go with **B** for the fixed-count buffers and the ring for per-object (§6). B does not help the
per-object case at all: it would turn hundreds of committed 64 KB-minimum allocations into hundreds
× N, which is the memory problem §10.3 of the architecture doc names, made worse.

Revisit if you ever add a second per-draw constant buffer — at that point the ring is the only
sensible home and B's remaining users are few enough to migrate.

### 5b. Structure

```cpp
class ConstantBuffer : public GPUResource
{
public:
    ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT alignedSizePerFrame);
    ~ConstantBuffer() override;

    // Writes the current frame slot only. Correct for anything rewritten every frame.
    void Update(void* data, UINT size);

    // Writes every slot. Use for values changed outside the per-frame write window —
    // load-time material setup, and anything set before the first BeginFrame.
    void UpdateAllFrames(void* data, UINT size);

    // Base address of the current frame's sub-range.
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const override;

    // Set once per frame by ResourceManager::BeginFrame, before any Update call.
    static void SetFrameSlot(UINT slot) { s_FrameSlot = slot; }

private:
    uint8_t* m_MappedBuffer;
    UINT m_AlignedSizePerFrame;
    static UINT s_FrameSlot;
};
```

Two notes on the declaration. `m_MappedBuffer` changes from `void*` to `uint8_t*` so slot offsets
are legal pointer arithmetic. And `GetGPUAddress` must be marked `virtual` in `GPUResource.h` —
`GPUResource` is already polymorphic (virtual destructor), so this costs nothing, and it is the
difference between a clean override and a name-shadowing bug that fires only when someone binds
through a `GPUResource*`.

### 5c. Lifecycle and the ordering hazard

`ResourceManager::CreateConstantBuffer` (line 183) aligns to
`D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT` and creates a committed upload resource of
`alignedSize`. Change one line — the resource `Width` becomes `alignedSize * FRAMES_IN_FLIGHT` —
and pass `alignedSize` (per-frame, **not** total) to the constructor. Getting that backwards gives
you a buffer N× too small with every slot pointing into it: the debug layer will not catch it.

The ordering hazard: **`s_FrameSlot` must be set before any `Update` in that frame.** Two current
writers violate this if you do nothing:

- **`Renderer::UpdatePostProcessingCB`** (line 646) is called from `SetOptions`, which the ImGui
  debug UI invokes via `ApplyRendererOptions` during `m_UISystem->BeginFrame(...)` — *before*
  `ExecutePipeline`. It would write the previous slot and the frame would bind the current one.
  Fix: add `bool m_OptionsDirty` set in `SetOptions`, and call `UpdatePostProcessingCB()` from
  `ExecutePipeline` after the slot advance, when dirty.
- **`Material::UpdateConstantBufferData`** (`Material.cpp:25`) is called from ~20 `PBRMaterial`
  setters, all at asset-load time, most of them before the first frame. Point it at
  `UpdateAllFrames`. Material data is small and changes rarely; the theoretical torn read if a
  setter fires mid-frame is not worth a deferred-apply queue here.

### 5d. Core logic

```
Update(data, size):
    assert size <= m_AlignedSizePerFrame
    memcpy(m_MappedBuffer + s_FrameSlot * m_AlignedSizePerFrame, data, size)

UpdateAllFrames(data, size):
    assert size <= m_AlignedSizePerFrame
    for slot in 0 .. FRAMES_IN_FLIGHT-1:
        memcpy(m_MappedBuffer + slot * m_AlignedSizePerFrame, data, size)

GetGPUAddress():
    return m_GPUAddress + s_FrameSlot * m_AlignedSizePerFrame   // m_GPUAddress is the resource base
```

Notes:

- Keep the existing `EngineUtils::Assert(size <= ...)` guard from `ConstantBuffer.cpp:23` — with
  N slots, an over-long write now silently corrupts the *next slot* rather than running off the end
  of the resource, so the assert is doing more work than before.
- `m_GPUAddress` is set by the `GPUResource` base constructor and is the resource base; the offset
  arithmetic is yours. CBV addresses must be 256-byte aligned, which they are because
  `m_AlignedSizePerFrame` already is.
- `s_FrameSlot` is a plain `static UINT` — single-threaded recording, no atomic needed. If you ever
  move to multi-threaded recording it becomes a real problem; note it and move on.

### 5e. Wiring

In `ResourceManager`, add `void BeginFrame(UINT frameSlot);` — for now it just calls
`ConstantBuffer::SetFrameSlot(frameSlot)`; §6 adds the ring reset to the same function. Call it from
`Renderer::ExecutePipeline` immediately next to the existing
`m_RenderContext->GetHeapManager().BeginFrame(m_FrameIndex++)` (line 209), and restructure that line
so the slot is computed once:

```
slot = m_FrameIndex % FRAMES_IN_FLIGHT
heapManager.BeginFrame(m_FrameIndex)
ResourceManager::GetInstance().BeginFrame(slot)
```

Leave `m_FrameIndex++` for slice 4, where the gate needs it in a specific place.

### 5f. Checkpoint

Identical image. If geometry explodes or materials go magenta, an address is being computed against
the wrong base — check the `alignedSize` vs `alignedSize * N` split in 5c first.

---

## 6. Slice 3 — `FrameConstantAllocator`

### 6a. Why this shape

`RenderComponent::Init` (line 29) creates one committed upload resource per primitive binding.
Committed buffers carry a 64 KB minimum allocation granularity, so a 464-byte struct costs 64 KB of
address space each; a scene with 500 bindings burns ~32 MB and 500 `CreateCommittedResource` calls
at load. The fix is the standard one: a few big upload buffers, bump-allocated per draw, reset per
frame.

You already have a working example of exactly this in the repo. `RmlRenderInterfaceDX12` keeps
persistently-mapped `m_TransientVertexBuffer` / `m_TransientIndexBuffer` / `m_ConstantBuffer` with
`m_TransientVertexOffset` / `m_ConstantBufferOffset` bump cursors, reset at
`ResetTransientBufferOffsets()` (`RmlRenderInterfaceDX12.cpp:539`) and advanced after each draw
(line 657). **Same skeleton, two deltas:** N slots instead of one, and no growth path (throw on
overflow instead of reallocating, because reallocating mid-frame would invalidate addresses already
recorded into a command list).

### 6b. Structure

New pair in `src/DX12Engine/Rendering/Buffers/`, next to `ConstantBuffer.h`:

```cpp
class FrameConstantAllocator
{
public:
    FrameConstantAllocator(ID3D12Device* device, UINT bytesPerFrame);
    ~FrameConstantAllocator();
    FrameConstantAllocator(const FrameConstantAllocator&) = delete;
    FrameConstantAllocator& operator=(const FrameConstantAllocator&) = delete;

    // Resets the cursor for this slot. Only legal once the slot's frame has retired.
    void BeginFrame(UINT frameSlot);

    // Copies size bytes into the current slot and returns a 256-byte-aligned CBV address.
    D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* data, UINT size);

    UINT GetPeakBytesUsed() const { return m_PeakBytesUsed; }

private:
    struct FrameSlot
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> Buffer;
        uint8_t* Mapped = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS BaseAddress = 0;
        UINT Cursor = 0;
    };

    FrameSlot m_Slots[FRAMES_IN_FLIGHT];
    UINT m_CurrentSlot = 0;
    UINT m_BytesPerFrame = 0;
    UINT m_PeakBytesUsed = 0;
};
```

Construction: one `CreateCommittedResource` per slot, `D3D12_HEAP_TYPE_UPLOAD`,
`D3D12_RESOURCE_STATE_GENERIC_READ`, `CD3DX12_RESOURCE_DESC::Buffer(bytesPerFrame)` — the same
recipe as `ResourceManager::CreateConstantBuffer` (lines 188–214), minus the per-CB descriptor
fiddling. `Map(0, nullptr, ...)` once and never unmap until the destructor;
`GetGPUVirtualAddress()` once and cache it. Route the `HRESULT`s through
`EngineUtils::ThrowIfFailed` per §7 of the style guide.

### 6c. Core logic

```
BeginFrame(frameSlot):
    m_CurrentSlot = frameSlot
    m_PeakBytesUsed = max(m_PeakBytesUsed, m_Slots[frameSlot].Cursor)   // record before clearing
    m_Slots[frameSlot].Cursor = 0

Allocate(data, size):
    slot        = m_Slots[m_CurrentSlot]
    alignedSize = AlignUINT(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)   // 256

    if slot.Cursor + alignedSize > m_BytesPerFrame:
        throw runtime_error("Frame constant ring exhausted; raise CONSTANT_RING_BYTES_PER_FRAME.")

    offset = slot.Cursor                       // capture BEFORE advancing — this is the returned address
    memcpy(slot.Mapped + offset, data, size)   // copy `size`, not `alignedSize`: the tail is padding
    slot.Cursor += alignedSize                 // advance by the ALIGNED size, or the next CBV is misaligned

    return slot.BaseAddress + offset
```

Notes:

- `EngineUtils::AlignUINT` already exists (`EngineUtils.h:63`). Use it rather than a local
  round-up, so the alignment rule has one definition.
- The throw-on-overflow mirrors `RenderPassDescriptorHeap::AllocateHandleBlock` (line 34) —
  same failure mode, same style of message. Do not silently wrap: wrapping hands two draws the same
  address and produces objects wearing each other's transforms, which is a miserable bug to find.
- `m_PeakBytesUsed` is your sizing evidence. Print it next to the descriptor-heap stats in the
  `#ifdef _DEBUG` block at `Renderer.cpp:609` so you can right-size the constant later.

### 6d. Wiring into `RenderComponent`

Three edits, and one deletion that does most of the work:

1. `ResolvedPrimitiveBinding` (`RenderComponent.h:36`): **delete**
   `std::unique_ptr<ConstantBuffer> PrimitiveConstantBuffer;`. Keep `CBVAddress` — it stops being a
   creation-time constant and becomes a per-frame output.
2. `RenderComponent::Init` (line 25): the loop no longer creates a CB. It still resets
   `PrevUnjitteredMVPMatrix` / `HasValidPrevUnjitteredMVP`, so do not delete the whole function.
3. `RenderComponent::UpdateConstantBufferData` (line 129): the guard
   `if (!binding.PrimitiveConstantBuffer) return;` goes, and the tail
   `binding.PrimitiveConstantBuffer->Update(...)` becomes
   `binding.CBVAddress = ResourceManager::GetInstance().AllocateFrameConstants(&m_RenderObjectData, sizeof(RenderComponentData));`

`ResourceManager` gains the ring as a `std::unique_ptr<FrameConstantAllocator>` member, constructed
in `Init(RenderContext&)` (line 58, where `m_Device` is already available), plus a thin
`AllocateFrameConstants` forwarder and the `BeginFrame(slot)` from §5e now also calling
`m_ConstantAllocator->BeginFrame(slot)`.

**Why `ResourceManager` and not `RenderContext`:** `RenderContext` owns the device and the other
per-frame machinery, which makes it the more natural owner. But `RenderComponent` holds no
`RenderContext` — it reaches everything through `ResourceManager::GetInstance()`. Putting the ring
on `RenderContext` means threading a context pointer into every `RenderComponent`, which is a
bigger, more invasive change for an ownership nicety. Take `ResourceManager`.

### 6e. The one thing that could break

`DrawItem::CBVAddress` (`Renderer.cpp:439`) is both a **sort key** (line 497) and the **lazy-bind
dedup key** in `GeometryRenderPass` (line 122). Both still work: within a frame the ring hands out
unique, stable addresses, and `SetSceneData` fills every `CBVAddress` before any pass runs. What
changes is that the sort now orders by allocation order rather than by object identity — harmless
for correctness, and the primary/secondary keys (PSO, material, mesh) still do the batching work.

The ordering hazard: **`SetSceneData` must run after `BeginFrame`.** It already does
(`ExecutePipeline` lines 209 and 213), but if you reorder that function, every object's constants
land in the slot the *previous* frame is still reading.

### 6f. Checkpoint

Identical image. Check Task Manager or PIX: process VRAM should drop by roughly
`64 KB × bindingCount − FRAMES_IN_FLIGHT × 4 MB`. Load time should improve slightly.

---

## 7. Slice 4 — remove the stall

### 7a. Structure

Add to `Renderer`:

```cpp
UINT m_FrameFenceValues[FRAMES_IN_FLIGHT] = {};
```

### 7b. Core logic

At the very top of `ExecutePipeline`, before the resize check and before anything resets a cursor:

```
slot = m_FrameIndex % FRAMES_IN_FLIGHT

// Frame (m_FrameIndex - FRAMES_IN_FLIGHT) owned this slot. Wait for it, and only it.
if m_FrameFenceValues[slot] != 0:
    graphicsQueue.WaitForFenceCPUBlocking(m_FrameFenceValues[slot])

if IsPendingResize: OnResize(pipeline)          // after the gate: OnResize drains all queues anyway
heapManager.BeginFrame(m_FrameIndex)             // resets the transient descriptor cursor for `slot`
ResourceManager::GetInstance().BeginFrame(slot)  // resets the ring + CB slot
if m_OptionsDirty: UpdatePostProcessingCB(); m_OptionsDirty = false
... SetSceneData, passes, PresentFrame ...
```

And in `PresentFrame`, replace lines 603–606:

```
fenceVal = graphicsQueue.ExecuteCommandList()
renderContext->PresentFrame()                    // Present(1,0)
m_FrameFenceValues[slot] = fenceVal              // record; do NOT wait
m_FrameIndex++                                   // advance here, once, after the frame is submitted
```

Notes:

- The gate waits on **one** slot, not on all outstanding work. That is the whole change: at
  `FRAMES_IN_FLIGHT = 2` the CPU may run one full frame ahead of the GPU.
- `slot` must be a member or a parameter — `PresentFrame` currently takes only the render target.
  A `UINT m_CurrentFrameSlot` member set in `ExecutePipeline` is the least invasive route.
- `m_FrameIndex++` moves out of the `heapManager.BeginFrame(m_FrameIndex++)` call (line 209) and to
  the end of `PresentFrame`. If you leave it in both places the slot rotation desynchronises from
  the fence array by one and you will chase a phantom.
- The `!= 0` guard covers the first `FRAMES_IN_FLIGHT` frames, when no fence has been recorded yet.
  Fence value 0 is never handed out — `CommandQueue` starts `m_NextFenceValue` at 1
  (`CommandQueue.cpp:17`) — so 0 is a safe sentinel.
- `OnResize` calls `WaitForAllIdle` transitively (`RenderContext.cpp:77`), which retires every
  recorded fence. The stale non-zero values left in `m_FrameFenceValues` are then already complete,
  so waiting on them is a cheap no-op — no need to clear the array, though clearing it is harmless
  and arguably clearer.

### 7c. Checkpoint, and what "working" looks like

Two things to be honest about here.

**Vsync will hide the win.** `RenderWindow::PresentFrame` calls `Present(1, 0)` (line 126) and the
flip-model swap chain enforces its own frame latency. Frame *rate* will not move. To see the change,
temporarily switch to `Present(0, 0)` and compare, or measure CPU-side: the wall time from the top
of `ExecutePipeline` to the end of `PresentFrame` should drop by roughly the GPU frame time. Put
`Present(1, 0)` back afterwards.

**Watch for the allocator stall.** If frame time does not move even with vsync off, you missed
§4c — the pool is still 8 and `ResetCommandAllocatorAndList` is doing the waiting the fence gate
used to do.

---

## 8. Slice 5 — non-blocking uploads

### 8a. Why this is nearly free

`GPUUploader::ExecuteUpload` (line 107) does four things in strict CPU order: execute the copy list,
**CPU-wait**, execute the graphics barrier list, **CPU-wait**. The second wait exists only to make it
safe to release the upload resources on the next two lines.

The GPU ordering you actually need is: *the barrier must not execute before the copy finishes, and
no pass sampling the texture may execute before the barrier.* The second half is already free —
everything renders on the graphics queue, and `UploadAllPending()` submits the barrier list before
the pass loop (`Renderer.cpp:216`) and again at the top of `PresentFrame` (line 570). The first half
is a cross-queue GPU wait, and `CommandQueue::InsertWaitForQueueFence` already exists
(`CommandQueue.cpp:63`) — `RmlRenderInterfaceDX12.cpp:764` uses it for exactly this hand-off.

So the only genuinely new machinery is retiring the upload resources by fence instead of by CPU wait.
And `RmlRenderInterfaceDX12` has that too: `QueueUploadResourceRelease` /
`ProcessPendingUploadReleases` (lines 188 and 202). Read those two functions before writing yours.

### 8b. Core logic

```
ExecuteUpload():
    if not m_UploadListsRecording: return

    copyFence = copyQueue.ExecuteCommandList()
    graphicsQueue.InsertWaitForQueueFence(&copyQueue, copyFence)   // GPU-side; queued, not blocking
    graphicsFence = graphicsQueue.ExecuteCommandList()

    m_UploadListsRecording = false

    // Retire by fence instead of by CPU wait.
    QueuePendingReleases(graphicsFence)   // moves m_PendingUploadResources + m_PendingReferencedResources
                                          // into m_RetiringResources with this fence value
    ProcessRetiredResources()             // release everything whose fence <= PollCurrentFenceValue()
```

Notes:

- **Order matters:** `InsertWaitForQueueFence` must be issued on the graphics queue *before* its
  `ExecuteCommandList`. It is a queue operation, not a command-list operation — it applies to work
  submitted after it.
- `ProcessRetiredResources()` should also be called once per frame from somewhere cheap (the top of
  `UploadAllPending`, or next to the debug stats block) so resources are freed even in frames with
  no uploads.
- `UploadTextureBatch` (line 32) sets `texture->SetIsReady(true)` and writes the persistent SRV
  *before* `ExecuteUpload`. That stays correct: `CreateShaderResourceView` is a CPU-side descriptor
  write with no dependency on the copy, and the GPU ordering is enforced by the cross-queue wait.
  Resist the urge to "fix" it.
- The destructor must not release resources the GPU may still be reading. `~GPUUploader` currently
  calls the two release functions directly (line 28). Have it `WaitForIdle()` on the graphics queue
  first, then drain — matching `RmlRenderInterfaceDX12::Shutdown` (line 82).

### 8c. Checkpoint

Fly the demo camera to bring an un-streamed material into view for the first time. Before this
slice: a visible single-frame hitch. After: none. The debug layer must stay silent — a released
upload heap still in use is exactly the class of error it reports loudly.

---

## 9. Slice 6 — flip to three

Change one line in `Utils/Constants.h`. Then verify, in order:

1. The swap chain reports 3 buffers and `m_RenderTargets[2]` is populated (§4d).
2. The transient descriptor heap is `512 × 3` — `RenderPassDescriptorHeap` sizes itself from the
   constant (`.cpp:9`) with no edit needed.
3. ImGui gets `NumFramesInFlight = 3` — `ImGuiDebugBackend.cpp:304` reads the same constant, also
   free.
4. The ring costs 12 MB instead of 8.

Measure with vsync off. At this scale N = 3 usually buys little over N = 2 and adds a frame of input
latency; keep 2 unless the numbers say otherwise. The point of the slice is that the choice is now
one line, not a refactor.

---

## 10. Wiring checklist

Mechanical steps, in order — this is the section that is easy to skip and expensive to skip:

1. `Utils/Constants.h`: three new constants. `RenderPassDescriptorHeap.h`: include it, delete line 8.
2. `CommandQueue.cpp:11`: pool size from the constant.
3. `RenderWindow`: four sites from §4d.
4. `GPUResource.h:16`: `virtual` on `GetGPUAddress`.
5. `ConstantBuffer`: new members, `UpdateAllFrames`, `SetFrameSlot`, `GetGPUAddress` override; define
   `UINT ConstantBuffer::s_FrameSlot = 0;` in the `.cpp`.
6. `ResourceManager.cpp:191`: resource `Width` becomes `alignedSize * FRAMES_IN_FLIGHT`; constructor
   still receives the per-frame `alignedSize`.
7. New `FrameConstantAllocator.h` / `.cpp`.
8. `ResourceManager`: `std::unique_ptr<FrameConstantAllocator> m_ConstantAllocator` constructed in
   `Init`; `BeginFrame(UINT)`; `AllocateFrameConstants(const void*, UINT)`.
9. `RenderComponent`: delete the CB member, gut the `Init` allocation, repoint
   `UpdateConstantBufferData`.
10. `Material.cpp:27`: `UpdateAllFrames`.
11. `Renderer`: `m_FrameFenceValues[]`, `m_CurrentFrameSlot`, `m_OptionsDirty`; gate at the top of
    `ExecutePipeline`; stall removed and fence recorded in `PresentFrame`; `m_FrameIndex++` moved.
12. `GPUUploader`: async `ExecuteUpload`, retiring list, draining destructor.
13. `RmlRenderInterfaceDX12`: N-buffer the three transient regions (§13, first bullet).

## 11. Build system

**Nothing to do.** Both `CMakeLists.txt` files use `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so
`FrameConstantAllocator.h` / `.cpp` are picked up when CMake re-runs on the next build. If they are
not compiled, force a reconfigure:

```
cmake --build ../DX12Engine_build --target DemoScene --config Debug
```

## 12. Invariant checklist

Audit your own diff against these before you call it done:

- [ ] Every `CreateConstantBuffer` resource is `alignedSize * FRAMES_IN_FLIGHT` bytes wide, and the
      `ConstantBuffer` constructor receives the **per-frame** size.
- [ ] `ConstantBuffer::SetFrameSlot` is called exactly once per frame, before any `Update` in that
      frame. No `Update` call site runs earlier in the frame than `ExecutePipeline`.
- [ ] `Renderer::UpdatePostProcessingCB` is no longer reachable from `SetOptions` directly.
- [ ] The ring's `BeginFrame(slot)` happens after the fence gate for `slot` and before
      `SetSceneData`.
- [ ] `FrameConstantAllocator::Allocate` advances by the **aligned** size and returns the
      pre-advance offset.
- [ ] No `WaitForFenceCPUBlocking` remains in the steady-state frame path except the single frame
      gate. Grep: expect hits only in `CommandQueue.cpp`, `CommandQueueManager.cpp`, the gate, and
      `RmlRenderInterfaceDX12`'s texture-creation path.
- [ ] `COMMAND_ALLOCATOR_POOL_SIZE >= FRAMES_IN_FLIGHT × (passes + 1)` for the largest pipeline you
      run.
- [ ] Every per-frame cursor reset (`RenderPassDescriptorHeap`, the ring, Rml's transient offsets,
      Rml's `m_ConstantBufferOffset`) is either per-slot or fence-gated.
- [ ] Upload resources are released only after a fence proves the graphics queue passed them.
- [ ] `~GPUUploader` drains the queue before releasing.

## 13. Gotchas specific to this task

- **The Rml runtime UI is a live counter-example.** `RmlRenderInterfaceDX12::BeginFrame` calls
  `ResetTransientBufferOffsets()` and `m_ConstantBufferOffset = 0` (lines 157–158) on
  *single-buffered* upload resources. Today that is safe for the same reason everything else is:
  the stall. After slice 4 it is a live data race that shows as UI geometry tearing or flickering
  under load — and `uiConfig.EnableRuntimeUI = true` in the demo, so you will hit it. Fix it the
  same way as the ring: N slots, `BeginFrame(slot)`, per-slot cursors. The growth path in
  `EnsureTransientBuffers` (line 477) needs care — reallocating while an older frame's command list
  still references the old buffer requires deferring the release, which
  `QueueUploadResourceRelease` already gives you.
- **8 command allocators is the hidden stall.** Covered in §4c because it is worth two mentions:
  it makes the entire change look like it did nothing.
- **`m_FrameIndex` has two jobs.** It drives the descriptor heap's slot rotation
  (`BeginFrame(m_FrameIndex)` → `% FRAMES_IN_FLIGHT` inside) *and*, after slice 4, indexes the fence
  array. It must be incremented exactly once per frame. The current `m_FrameIndex++` is buried in an
  argument at `Renderer.cpp:209`; move it, do not duplicate it.
- **Fence values are `UINT` at the API boundary but `UINT64` internally.** `ExecuteCommandList`
  returns `static_cast<UINT>(fenceValue)` (`CommandQueue.cpp:103`) while `m_NextFenceValue` is
  `UINT64`. At ~10 submissions per frame at 60 Hz this wraps after roughly 80 days of continuous
  running. Not worth fixing now; worth knowing before you debug an impossible frame gate.
- **The transient descriptor heap capacity is now a real ceiling.**
  `SRV_TRANSIENT_CAPACITY_FRAME = 512` was previously exercised by one frame at a time in a
  serialised world. It still is — the heap is per-slot — but `AllocateHandleBlock` *throws* on
  overflow (`RenderPassDescriptorHeap.cpp:34`), and an exception thrown mid-frame with a partially
  recorded command list is a much worse failure than it used to be. Watch the 80%/95% warnings
  already printed at `Renderer.cpp:615`.
- **Do not "optimise" the ring by returning an address before copying.** The copy must complete on
  the CPU before the address is recorded into a command list, which it does trivially here — but if
  you ever split `Allocate` into `Reserve` + `Write`, that stops being true.
- **`GetGPUAddress` is called on `ConstantBuffer` through several shapes** — raw pointer
  (`ConstantBuffer* cascadedShadowCB`), reference (`GetScreenDataBuffer()`), and `unique_ptr`.
  All resolve to `ConstantBuffer`, so shadowing would *appear* to work. Making it `virtual` is what
  keeps it working when someone later stores one as a `GPUResource*`.
- **`OnResize` runs inside the gated region.** It drains all queues, which is heavier than the gate
  but correct. Do not move it above the gate to "save time" — it releases swap-chain buffers, and
  the gate is what proves no in-flight frame still references them.

## 14. Validation

Do these by hand, in order, after slice 5. There is no test suite — running `DemoScene` is the
instrument.

- **Steady state.** Run `../DX12Engine_build/DemoScene/Debug/DemoScene.exe`, orbit the camera for
  ~30 s. Expect: image identical to `master`, debug layer silent, no output-window warnings.
- **The measurement.** Temporarily `Present(0, 0)`. Compare CPU frame time (the ImGui debug overlay
  already shows `FrameTimeMs`) before and after slice 4. Expect a drop toward the larger of the CPU
  and GPU times rather than their sum — on a GPU-bound scene, close to 2×. If it does not move,
  re-read §4c.
- **The corruption test.** Move fast through a scene with animated objects and a shadow-casting
  directional light. Stale constants show as an object's shadow lagging one frame behind the object,
  or an object's velocity buffer disagreeing with its position — the latter appears as TAA ghosting
  that only occurs while moving. Toggle TAA off in the debug UI: if the ghosting vanishes and
  geometry is fine, you have a `PrevMVPMatrix` timing bug, not a ring bug.
- **First-sighting hitch.** Turn to face un-streamed geometry. Before slice 5, `FrameTimeMs` spikes
  on the frame the material first appears; after, it should not.
- **Resize.** Drag the window edge continuously for several seconds. This exercises the gate, the
  queue drain, and the per-slot reset paths together. Expect no crash, no debug-layer complaint,
  and a correct image at the new size.
- **Ring headroom.** Read the `_DEBUG` peak print you added in §6c. If it is anywhere near
  `CONSTANT_RING_BYTES_PER_FRAME`, raise the constant before shipping.
- **N = 3.** Repeat the steady-state and resize checks with the constant flipped (§9).

## 15. Close-out

- **Update `docs/Rendering-Architecture.md`.** This change invalidates four sections, and leaving
  them stale is worse than not writing them: **§3.2** (the submission-model description and its
  "load-bearing for correctness" paragraph), **§3.3** (uploads are no longer synchronous),
  **§10.3** (per-primitive committed CBs are gone), **§14** (strike weaknesses 1, 5 and 6), and
  **§16** (mark Tier 2 items 4, 5 and 7 done). The comparison table in §12 gains a row change for
  "Submission".
- **Style.** Tabs, Allman braces, `m_PascalCase` members, `PascalCase` public struct fields, one
  project namespace — `CPP-STYLE-GUIDE.md` §1–§3. New files are `PascalCase.h`/`.cpp` named for
  their primary type. Comment only the non-obvious *why* (§9 of the style guide): the fence-gate
  arithmetic and the "advance by aligned size" line earn one line each; the memcpys do not.
- **Commit.** The repo's history is one commit per coherent milestone with a descriptive summary
  (`3f616ee Rendering fixes 1: Add tone mapping, reverse depth, shadow mapping and caching fixes`).
  Match that: one commit per slice group, or a single `Rendering fixes 2: Frame pipelining, ring
  constant allocator and async uploads` if you prefer the history the doc's tiers imply.
- **No tests to add** — there is no test suite. The validation pass in §14 is the acceptance gate.

## 16. Optional / future

- **Waitable swap chain.** `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` +
  `SetMaximumFrameLatency(1)` gives markedly better input latency at the same throughput. Costs a
  move to `CreateSwapChainForHwnd` and a wait handle in the frame loop.
- **Suballocate vertex/index buffers too.** `CreateVertexBuffer` / `CreateIndexBuffer`
  (`ResourceManager.cpp:71`, `:127`) each create *two* committed resources. A placed-resource heap
  would cut load-time allocation count sharply. Independent of this work.
- **Multi-threaded pass recording.** The natural next step once frames overlap, and the point at
  which `ConstantBuffer::s_FrameSlot` being a plain `static` stops being acceptable. Requires one
  command allocator *per thread* per frame, which the pool from §4c does not currently model.
- **Async compute for shadows/SSR.** `CommandQueueManager` already owns a compute queue that nothing
  uses. Tier 3 item 10.
