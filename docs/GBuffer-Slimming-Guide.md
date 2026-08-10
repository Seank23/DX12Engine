# Tier 2b — Slimming the G-buffer: Implementation Guide

A step-by-step guide for implementing item **6** of the Tier 2 roadmap in
`docs/Rendering-Architecture.md` yourself: delete the redundant world-position target, octahedral-
encode the normals, and narrow the material target. It assumes you know HLSL and the deferred
pipeline in this repo, so it concentrates on **where the change leaks** — the register renumbering
that a shrinking descriptor table forces on two shaders, and the three places a render-target format
is declared that must all agree.

It does not hand you finished shader bodies. The octahedral encode/decode pair and the depth→world
reconstruction are given as formulas and pseudocode with their traps flagged; you write them.

Items 4, 5 and 7 (frame pipelining, ring allocator, async uploads) are covered separately in
`docs/Frame-Pipelining-Guide.md`. The two guides touch disjoint files and can be done in either
order.

## What we're building

The G-buffer goes from **8 targets at 52 bytes/pixel** to **6 targets at 32 bytes/pixel** — a 38%
bandwidth and memory cut, ~107 MB → ~66 MB at 1080p, in four steps:

| Target | Now | After | Δ |
|---|---|---|---:|
| Albedo | `R8G8B8A8_UNORM` | unchanged | 0 |
| WorldNormal | `R16G16B16A16_FLOAT` | merged ↓ | |
| ObjectNormal | `R16G16B16A16_FLOAT` | **`Normals` `R16G16B16A16_UNORM`** (`.xy` world oct, `.zw` object oct) | −8 |
| Material | `R16G16B16A16_FLOAT` | **`R8G8B8A8_UNORM`** | −4 |
| **Position** | `R16G16B16A16_FLOAT` | **deleted** — reconstructed from depth | −8 |
| Emissive | `R16G16B16A16_FLOAT` | unchanged (carries AO in `.a`) | 0 |
| Velocity | `R16G16_FLOAT` | unchanged | 0 |
| Depth | `D32_FLOAT` | unchanged | 0 |
| | **52 B/px** | **32 B/px** | **−20** |

Deleting Position is also a **quality** improvement, not only a saving. Half-float world coordinates
have ~11 bits of mantissa, so beyond a few thousand world units the stored position quantises to
worse than 1 unit — while `D32_FLOAT` depth run back through the inverse projection is exact to
sub-millimetre at those distances. Any position-dependent artefact you have at range gets better.

## Why this one is different

Everything here is arithmetic you can verify against a reference — but the *plumbing* is where it
bites, because the pass graph derives shader registers from data:

- **Shader registers are computed, not declared.** `RenderPass::Init` walks `m_ResourceBlockOrder`
  and assigns `baseRegister += blockSize` per block (`RenderPass.cpp:19–28`). Remove one slot from
  the G-buffer block and *every texture after it in the root signature slides down one register* —
  in two different shaders, with two different layouts. Nothing warns you; you get a shadow map
  bound where a depth buffer should be, which reads as "shadows broke" rather than "registers
  shifted".
- **Each render-target format is declared in three places that must agree.** The
  `RenderTextureConfig` in `GeometryRenderPass::Init`, the `SetRenderTargets({...})` list in
  `GeometryRenderPass::CreatePSO`, and — easy to miss — a **duplicate copy** of that list in
  `MaterialTemplate::BuildPSODesc` (`MaterialTemplate.cpp:109`). Miss the third and every object
  using a material PSO variant breaks while everything else looks right.
- **Encoding introduces a unit boundary.** Octahedral coordinates live in `[-1,1]`; a `UNORM`
  target stores `[0,1]`. The `*0.5+0.5` / `*2-1` pair must exist on both sides exactly once. Applied
  twice or zero times, normals still *look* plausible — lighting just goes subtly wrong in a way
  that is hard to attribute.

> **Design decisions (settled)**
>
> - One shared `Shaders/Common/GBufferUtils.hlsli` holds reconstruction and oct encode/decode, so
>   the lighting and SSR passes can never disagree (§4a).
> - Normals merge into **one** `R16G16B16A16_UNORM` target rather than two `R16G16_UNORM` targets:
>   same bytes, one fewer RTV, barrier, descriptor and register (§7a).
> - `UNORM` not `FLOAT` for encoded normals — uniform 16-bit precision across the range is exactly
>   what octahedral mapping wants; `FLOAT16` wastes its exponent (§6a).
> - The reconstruction helper reproduces `SSRPass_PS.hlsl:64`'s expression **verbatim**, quarter-
>   texel nudge included, so slice 1 is provably a no-op for SSR (§4c).

> **Still-open micro-forks — my defaults, veto cheaply**
>
> - `ResourceSlot` gains `Normals` and loses `WorldNormal`, `ObjectNormal`, `Position` (§7b). The
>   alternative — reusing `WorldNormal` as the packed slot — is fewer edits but a lying name.
> - Emissive stays `R16G16B16A16_FLOAT`. Narrowing it to `R11G11B10_FLOAT` needs a new home for AO
>   and is a separate change (§15).
> - Slice 5 (`Material` → `RGBA8`) is the one step with a visible risk (roughness banding). If you
>   want the guaranteed-safe subset, stop after slice 4 and take 36 B/px.

> **The invariant contract for this task**
>
> After every slice the rendered image is either identical or better — this is a representation
> change, not a look change. Every G-buffer slot has exactly one producer write and one consumer
> read at a register both sides agree on. No shader keeps its own private copy of the reconstruction
> or encode maths.

---

## 1. Scope

In scope: the geometry pass's render-target set, `Geometry_PS`, the two shaders that consume the
G-buffer (`PBRLightingDeferred_PS`, `SSRPass_PS`), the `ResourceSlot` vocabulary, and the demo's
binding lists.

**Untouched — if your diff reaches these, you have overreached:**

- `Geometry_VS.hlsl`. It still needs `worldPos` in the interpolants for tangent-space work; only
  the *pixel shader's output* of it goes away.
- `TAAPass_PS.hlsl`, `FinalRender_PS.hlsl`, `PBRTransparent_PS.hlsl`. TAA reads only scene colour,
  depth, velocity, the reactive mask and its own history (`t0`–`t4`); the transparent pass is
  forward and gets world
  position from its own VS. Neither touches a slot you are changing.
- All shadow passes and `Shadows/*.hlsli`.
- The submission model, constant buffers, descriptor heaps, and everything in
  `docs/Frame-Pipelining-Guide.md`.
- `AssetCooker` and anything under `res/`. No asset changes.

## 2. Files you'll touch

| File | Change |
|---|---|
| `src/DX12Engine/Shaders/Common/GBufferUtils.hlsli` | **New** — `ReconstructViewPos`, `ReconstructWorldPos`, `OctEncode`/`OctDecode`, `PackNormal`/`UnpackNormal` |
| `src/DX12Engine/Shaders/Geometry_PS.hlsl` | `PSOutput` 7 targets → 5; drop `position`; write packed normals; keep velocity last |
| `src/DX12Engine/Shaders/PBRLightingDeferred_PS.hlsl` | Reconstruct `worldPos`; unpack normals; renumber `t2`–`t11` → `t2`–`t9` |
| `src/DX12Engine/Shaders/SSRPass_PS.hlsl` | Same reconstruction/unpack; renumber `t1`–`t9` → `t1`–`t7`; delete the local `ReconstructViewPos` (line 64) in favour of the shared one |
| `src/DX12Engine/Rendering/RenderPass/GeometryRenderPass.cpp` | `Init` target list; `Execute`'s hardcoded `7`/`8` counts; `GetRenderTarget` mapping; `CreatePSO` format list |
| `src/DX12Engine/Asset/MaterialTemplate.cpp` | The **duplicate** opaque format list at line 109 |
| `src/DX12Engine/Rendering/RenderPipelineConfig.h` | `ResourceSlot`: add `Normals`, remove `WorldNormal`, `ObjectNormal`, `Position` |
| `DemoScene/src/DX12EngineDemoApp.cpp` | `gBufferTypes` (line 92) and `ssrGBufferTypes` (line 115) |
| `docs/Rendering-Architecture.md` | §4.1 table, §14 item 4, §16 Tier 2 item 6 — at close-out |
| `CMakeLists.txt` | **No edit** — shaders are globbed with `CONFIGURE_DEPENDS` and staged by the post-build copy |

Absent by design: `LightingRenderPass.cpp` and `SSRRenderPass.cpp`. Neither names a G-buffer slot —
their descriptor tables are built from `m_ResourceBlocks` sizes, which come from the demo's binding
lists. That is the pass graph earning its keep.

## 3. Suggested build order

Five slices. Slices 1 and 3 are pure additions that leave the old data in place, so you can diff
against ground truth before deleting anything — take that opportunity, because after slice 2 there
is nothing left to compare to.

1. **Reconstruction, proven against the Position target.** Add the helper header; switch the
   lighting pass to reconstruct while `positionMap` still exists. Build, run, A/B. Nothing shrinks
   yet; this proves the maths in isolation.
2. **Delete the Position target.** 52 → 44 B/px. The first register renumbering. One axis of risk:
   plumbing.
3. **Octahedral-encode both normals in place.** Two `R16G16_UNORM` targets. 44 → 36 B/px. No
   register changes — the block is still 6 wide. One axis of risk: encoding maths.
4. **Merge the two normal targets into one.** 36 B/px unchanged, but 7 targets → 6, and the second
   register renumbering. One axis of risk: plumbing again.
5. **Material → `R8G8B8A8_UNORM`.** 36 → 32 B/px. Look for banding; this is the only slice that can
   plausibly cost quality.

Slices 2 and 4 are the ones to bisect to. Commit each separately.

---

## 4. Slice 1 — reconstruction, proven

### 4a. Why a shared header

Two shaders need world position from depth, and they are already inconsistent: `SSRPass_PS.hlsl:64`
has `ReconstructViewPos`, `PBRLightingDeferred_PS.hlsl:58` has a different `GetViewRay` used only
for the sky. If each grows its own world-space version they will drift, and a half-texel disagreement
between the lighting pass's shadow lookup and SSR's ray origin is precisely the kind of bug that
gets blamed on the shadow bias for a week.

The repo already has `Shaders/Common/ColorUtils.hlsli` and `Shaders/Lighting/PBRShading.hlsli` as
shared includes — follow that pattern. Note the include-order constraint: these helpers read
`InvProjectionMatrix`, `InvViewMatrix` and `ScreenSize` from the `ScreenBuffer` cbuffer, so
`GBufferUtils.hlsli` must be included **after** the cbuffer declaration, exactly as
`ShadowSampling.hlsli` is included after the texture declarations it uses
(`PBRLightingDeferred_PS.hlsl:55`).

### 4b. Structure

```hlsl
// Shaders/Common/GBufferUtils.hlsli
// Include AFTER the ScreenBuffer cbuffer declaration -- reads InvProjectionMatrix,
// InvViewMatrix and ScreenSize from it.

float3 ReconstructViewPos(float2 uv, float depth);    // view-space position of this pixel
float3 ReconstructWorldPos(float2 uv, float depth);   // = mul(InvViewMatrix, float4(viewPos, 1)).xyz

float2 OctEncode(float3 n);        // unit vector -> [-1,1]^2
float3 OctDecode(float2 oct);      // [-1,1]^2 -> unit vector

float2 PackNormal(float3 n);       // OctEncode then map to [0,1] for UNORM storage
float3 UnpackNormal(float2 stored); // map from [0,1] then OctDecode
```

`Pack`/`Unpack` exist as a separate layer on purpose: the `*0.5+0.5` / `*2-1` pair is the trap
(see §6c), and giving it exactly one home is how you stop it being applied twice.

### 4c. The reconstruction, and the nudge

Copy the body of `SSRPass_PS.hlsl:64–71` **verbatim** into `ReconstructViewPos`, including
`posCS.xy += 0.5 / ScreenSize;`. Then delete the local copy in `SSRPass_PS.hlsl` and include the
header.

That nudge is worth understanding before you decide to keep it. One pixel spans `2 / ScreenSize` in
NDC, so half a pixel is `1 / ScreenSize` — `0.5 / ScreenSize` is a *quarter* pixel, applied before
the `y *= -1` flip so the two axes end up nudged in opposite directions. It looks like a half-texel
correction that lost a factor of two.

**Keep it anyway for now.** SSR's heuristics are hand-tuned around current behaviour, and slice 1's
whole value is being a provable no-op for SSR so that any regression you see is attributable to the
lighting pass alone. Log it as a follow-up (§14), fix it on its own with an SSR before/after.

`ReconstructWorldPos` is then one line on top. Follow the engine's matrix convention — `mul(matrix,
vector)`, matrix first, as at `CascadedShadowSampling.hlsli:71` and `PBRLightingDeferred_PS.hlsl:62`.
Getting it backwards gives you a transposed transform, which produces a scene that is recognisably
*wrong* rather than subtly wrong, so this one at least fails loudly.

Reverse-Z needs no special handling here: the depth value goes into `posCS.z` unmodified and the
inverse of the same projection matrix that produced it undoes it. What *does* matter is **which**
projection matrix. `ScreenData.ProjectionMatrix` is the **jittered** one when TAA is enabled
(`RenderContext.cpp:110` takes `projectionOverride`), and `InvProjectionMatrix` is the inverse of
that same jittered matrix (line 112) — which is correct, because the depth buffer was rasterised
with the jitter. Do not "fix" this by reaching for an unjittered matrix; you would introduce a
sub-pixel skew that only appears with TAA on.

### 4d. Wiring for the A/B check

In `PBRLightingDeferred_PS.hlsl`, replace line 80:

```
// was: float3 worldPos = positionMap.Sample(samp, input.texCoord).rgb;
float3 worldPos = ReconstructWorldPos(input.texCoord, depth);
```

`depth` is already read on line 79, above it. Leave `positionMap` declared and bound — you are not
deleting anything yet.

### 4e. Checkpoint

This is the most valuable checkpoint in the guide, so do it properly. Build and run:

```
cmake --build ../DX12Engine_build --target DemoScene --config Debug
../DX12Engine_build/DemoScene/Debug/DemoScene.exe
```

Then, temporarily, return `float4(abs(worldPos - positionMap.Sample(samp, input.texCoord).rgb) * 100.0, 1.0)`
from the shader. Expect **black** everywhere except a faint tint at grazing angles and far distances
(where the half-float Position target is the one that is wrong). Bright patches, banding, or
anything structured means the reconstruction is off — check the matrix convention first, then the
`y *= -1`.

Revert the debug return. Shader hot-reload works in Debug builds
(`ResourceManager::ReloadChangedShaders`), so you can iterate on this without restarting.

---

## 5. Slice 2 — delete the Position target

### 5a. The producer side

Four edits in `GeometryRenderPass.cpp`, and they must all land together or the pass will not create:

1. **`Init` (line 33):** delete the Position `emplace_back`. Targets go 8 → 7.
2. **`Execute` (lines 55–79):** the hardcoded `7`s become 6. Rather than retyping the number,
   derive it once — `const size_t colorTargetCount = m_RenderTargets.size() - 1;` — and use it for
   the barrier loop, `OMSetRenderTargets`, the clear loop, and the depth target's index
   (`m_RenderTargets[colorTargetCount]`, which currently appears as the literal `7` three times).
   The `rtBarriers[8]` array bound can stay; it is now oversized, which is harmless.
3. **`GetRenderTarget` (line 167):** delete the `ResourceSlot::Position` case; renumber the indices
   after it — `Emissive` 5→4, `Velocity` 6→5, `Depth` 7→6.
4. **`CreatePSO` (line 203):** remove one `R16G16B16A16_FLOAT` from the list. Seven formats → six.

Then the fifth edit, in a different file: **`MaterialTemplate.cpp:109`** carries a duplicate of that
same format list for opaque material PSO variants. Remove the same entry there. If you skip this,
objects drawn through the pass's default PSO look right and objects drawn through a material
template's PSO do not — and since `MaterialTemplate::RebuildPipelineKey` sorts them into separate
batches, the failure is per-material and looks like a material bug.

`Geometry_PS.hlsl`: delete `float4 position : SV_Target4;` from `PSOutput` and renumber
`emissive` to `SV_Target4` and `velocity` to `SV_Target5`; delete the `output.position` write.
`input.worldPos` stays in `PSInput` — nothing else needs changing in `Geometry_VS.hlsl`.

### 5b. The register renumbering

This is the part to do with the table in front of you rather than from memory. The G-buffer
descriptor block shrinks from 7 to 6, and every block after it in `OrderedInputTypes`
(`RenderPipelineConfig.h:66`) slides down one register.

**`PBRLightingDeferred_PS.hlsl`** — blocks are EnvironmentMap(2), GBuffer(7→6), ShadowMap(1),
CubeShadowMap(1), CascadedShadowMap(1):

| Texture | Before | After |
|---|---|---|
| `environmentMap`, `irradianceMap` | `t0`, `t1` | unchanged |
| `albedoMap` | `t2` | `t2` |
| `worldNormalMap` | `t3` | `t3` |
| `objectNormalMap` | `t4` | `t4` |
| `materialMap` | `t5` | `t5` |
| ~~`positionMap`~~ | `t6` | **deleted** |
| `emissiveMap` | `t7` | **`t6`** |
| `depthMap` | `t8` | **`t7`** |
| `shadowMaps` | `t9` | **`t8`** |
| `shadowCubeMaps` | `t10` | **`t9`** |
| `cascadedShadowMaps` | `t11` | **`t10`** |

**`SSRPass_PS.hlsl`** — blocks are EnvironmentMap(1), GBuffer(7→6), SceneColor(1), then a manually
appended history table:

| Texture | Before | After |
|---|---|---|
| `environmentMap` | `t0` | unchanged |
| `albedoMap` … `materialMap` | `t1`–`t4` | unchanged |
| ~~`positionMap`~~ | `t5` | **deleted** |
| `emissiveMap` | `t6` | **`t5`** |
| `depthMap` | `t7` | **`t6`** |
| `pipelineOutputMap` | `t8` | **`t7`** |
| `historyMap` | `t9` | **`t8`** |

`historyMap` is the sharp edge. Its register is not written in the shader's terms at all — it comes
from `SSRRenderPass::CreatePSO:241`, which appends a table at base register
`(UINT)m_InputResources.size()`. That count drops from 9 to 8 automatically when the demo's
`ssrGBufferTypes` list loses an entry, so the C++ side needs **no edit** — but the HLSL register
declaration does. Change the shader and trust the count; do not hardcode 8 in the C++.

In `SSRPass_PS.hlsl` also replace `positionMap.Sample(...)` at line 483 with
`ReconstructWorldPos(texCoord, depth)`. `depth` is read on line 471 just above, and the sky
early-out at line 473 already guards the case where it is meaningless.

### 5c. The binding lists

`DX12EngineDemoApp.cpp`: remove `DX12Engine::ResourceSlot::Position` from `gBufferTypes` (line 97)
and from `ssrGBufferTypes` (line 120). Then remove `Position` from the `ResourceSlot` enum
(`RenderPipelineConfig.h:14`) so nothing can reference it again.

**Order matters:** the demo's list order *is* the register order. Both lists read Albedo,
WorldNormal, ObjectNormal, Material, Position, Emissive, Depth — deleting the fifth entry is
exactly what produces the tables in §5b. If you reorder while you are in there, the tables no
longer apply.

### 5d. Checkpoint

Image identical to slice 1. 52 → 44 B/px. If shadows have gone flat or SSR reflects the wrong thing,
you have a register off by one — go back to §5b rather than debugging the shadow maths.

---

## 6. Slice 3 — octahedral normals, in place

### 6a. Why `UNORM`, and why in place first

Octahedral mapping projects a unit vector onto the `|x|+|y|+|z| = 1` octahedron and unfolds it to a
square, giving a 2-component encoding with roughly uniform angular error — around 0.1° at 16 bits
per channel, well under any visible threshold for shading. It needs a format with **uniform**
precision across its range. `R16G16_FLOAT` spends its bits on exponent range it will never use and
degrades toward the ends; `R16G16_UNORM` gives a flat 1/65535 everywhere. Take `UNORM`.

Doing it in place — two targets of `R16G16_UNORM`, block still 6 wide — keeps this slice free of
register churn, so if the image changes you know it is the encoding and not the plumbing.

### 6b. The maths

Encode, given a normalised `n`:

```
n /= (abs(n.x) + abs(n.y) + abs(n.z))          // project onto the octahedron
if n.z >= 0:  oct = n.xy
else:         oct = (1 - abs(n.yx)) * sign(n.xy)   // note the SWIZZLE: yx, not xy
return oct                                      // in [-1, 1]^2
```

Decode is the inverse fold:

```
n = float3(oct.x, oct.y, 1 - abs(oct.x) - abs(oct.y))
t = saturate(-n.z)                              // how far below the equator we folded
n.xy += (n.xy >= 0 ? -t : t)                    // per-component, not a single scalar branch
return normalize(n)
```

Two traps in four lines:

- **`(1 - abs(n.yx))`** — the swizzle is `yx`. Writing `xy` gives an encoding that round-trips for
  the upper hemisphere and mirrors the lower one. Half your normals point the wrong way and it looks
  like a normal-map handedness bug.
- **`sign()` returns 0 at exactly 0.** For a normal lying precisely on an octahedron edge that
  collapses a component. In practice interpolated normals never land exactly on zero, but if you see
  a single-pixel seam along a perfectly axis-aligned face, this is why — a `>= 0 ? 1 : -1` form
  avoids it entirely.

### 6c. The `UNORM` boundary

```
PackNormal(n)      = OctEncode(n) * 0.5 + 0.5     // [-1,1] -> [0,1] for UNORM storage
UnpackNormal(s)    = OctDecode(s * 2.0 - 1.0)     // [0,1] -> [-1,1]
```

This pair must appear **exactly once each**, in the header. The failure mode when it is applied
twice is that all normals crowd toward `+Z`, which reads as "the scene looks flat and over-lit" —
not as an obvious bug. Write both functions before writing either call site.

### 6d. Producer and consumer edits

`Geometry_PS.hlsl`: the two normal outputs become `float2`, carrying `PackNormal(worldNormal)` and
`PackNormal(normalize(input.normal))`. Note the `normalize` on the object normal — the current code
writes `input.normal` raw (line 110), unnormalised after interpolation. Oct encoding *requires* a
unit vector, so this is a mandatory fix, and it slightly changes results at glancing angles for the
better.

`GeometryRenderPass.cpp`: both normal `RenderTextureConfig`s become `DXGI_FORMAT_R16G16_UNORM`
(`Init` lines 30–31), and the corresponding two entries in the `CreatePSO` format list. **And the
same two entries in `MaterialTemplate.cpp:109`** — third time this file appears, and it will not be
the last.

Consumers: `PBRLightingDeferred_PS.hlsl` lines 70–71 and `SSRPass_PS.hlsl` lines 302 and 319 become
`UnpackNormal(map.Sample(samp, uv).xy)`. Drop the surrounding `normalize()` — `OctDecode` already
returns a unit vector, and normalising twice is just wasted ALU.

### 6e. Checkpoint

Image should be **near**-identical: encoding is lossy, so expect a difference of at most a shade on
smooth curved highlights, and nothing on flat surfaces. 44 → 36 B/px. If the scene looks flat, see
§6c. If half of every object is lit wrong, see §6b.

---

## 7. Slice 4 — merge the normal targets

### 7a. Why merge

After slice 3 you have two `R16G16_UNORM` targets. Packing both into one `R16G16B16A16_UNORM`
costs the same 8 bytes/pixel but saves a render target: one fewer RTV, one fewer barrier per frame
in both directions, one fewer descriptor in every consumer's table, one fewer register, and one
fewer entry in two binding lists. At 6 targets the fixed per-target overhead is a real fraction of
the pass.

The alternative — leaving them separate — is defensible if you expect to add a G-buffer consumer
that wants only one of the two. Nothing in the roadmap does.

### 7b. The edits

`ResourceSlot` (`RenderPipelineConfig.h:8`): add `Normals`, remove `WorldNormal` and `ObjectNormal`.
Compile errors now point you at every remaining site, which is the cheapest way to find them.

`Geometry_PS.hlsl`: one `float4 normals : SV_Target1` carrying
`float4(PackNormal(worldNormal), PackNormal(objectNormal))`, and every later `SV_Target` index drops
by one. `PSOutput` is now five targets: albedo, normals, material, emissive, velocity.

`GeometryRenderPass.cpp`: one `R16G16B16A16_UNORM` config replaces the two; `GetRenderTarget` gains
a `Normals` case at index 1 and everything after it shifts down; `CreatePSO`'s list loses an entry.
And `MaterialTemplate.cpp:109`.

`DX12EngineDemoApp.cpp`: both lists replace two entries with one `ResourceSlot::Normals`.

### 7c. The second renumbering

G-buffer block 6 → 5. Same mechanism as §5b, one register further:

| Shader | Block layout | Result |
|---|---|---|
| `PBRLightingDeferred_PS` | Env(2), GBuffer(5), Shadow(1), Cube(1), CSM(1) | `t0`–`t1` env; `t2` albedo, `t3` normals, `t4` material, `t5` emissive, `t6` depth; `t7` shadowMaps, `t8` shadowCubeMaps, `t9` cascadedShadowMaps |
| `SSRPass_PS` | Env(1), GBuffer(5), SceneColor(1), +history | `t0` env; `t1` albedo, `t2` normals, `t3` material, `t4` emissive, `t5` depth; `t6` pipelineOutput; `t7` history |

Consumers now read both normals from one sample:

```
float4 packed       = normalsMap.Sample(samp, uv);
float3 worldNormal  = UnpackNormal(packed.xy);
float3 objectNormal = UnpackNormal(packed.zw);
```

That halves the normal fetches in the lighting pass, which is a small bonus.

### 7d. Checkpoint

Identical to slice 3. Still 36 B/px, but 7 targets → 6. Register mistakes here present as SSR
sampling the scene-colour buffer as if it were depth — a full-screen wash rather than a subtle
error, so it is at least obvious.

---

## 8. Slice 5 — narrow the material target

Roughness, metallic, clearcoat and clearcoat-roughness are all in `[0,1]`, and every consumer
already `saturate()`s them on read (`PBRLightingDeferred_PS.hlsl:73–76`, `SSRPass_PS.hlsl:308–311`).
`R8G8B8A8_UNORM` is the natural fit and what production deferred renderers use.

Three edits and no shader change at all: the `RenderTextureConfig` in `GeometryRenderPass::Init`,
the entry in `CreatePSO`'s format list, and — yes — `MaterialTemplate.cpp:109`.

**The one real risk in this guide.** 8 bits gives 256 roughness levels. Where SSR uses roughness it
squares it (`coneSpread = roughnessBias * roughnessBias * 0.4`, `SSRPass_PS.hlsl:93`), which makes
quantisation *finer* at low roughness where it matters most — that direction is fine. The exposure
is a large surface with a smooth roughness gradient: look for stepped bands in the specular
response, especially on the glossy floor materials. If you see them, this is the slice to revert;
you keep 36 B/px and lose nothing else.

36 → 32 B/px.

## 9. Wiring checklist

In order. The `MaterialTemplate` line appears four times because it is the edit most likely to be
missed:

1. New `Shaders/Common/GBufferUtils.hlsli`.
2. `SSRPass_PS.hlsl`: delete local `ReconstructViewPos`, include the header.
3. `PBRLightingDeferred_PS.hlsl`: include the header (after the `ScreenBuffer` cbuffer).
4. `Geometry_PS.hlsl`: `PSOutput` and the writes, once per slice.
5. `GeometryRenderPass.cpp`: `Init` configs, `Execute` counts, `GetRenderTarget` mapping,
   `CreatePSO` formats — once per slice.
6. `MaterialTemplate.cpp:109` — **once per slice**, matching `CreatePSO` exactly.
7. `RenderPipelineConfig.h`: `ResourceSlot` edits (slices 2 and 4).
8. `DX12EngineDemoApp.cpp`: both binding lists (slices 2 and 4).
9. Consumer shader register renumbering (slices 2 and 4) — from the tables, not from memory.

## 10. Build system

**Nothing to do.** `GLOB_RECURSE ... CONFIGURE_DEPENDS` picks up the new `.hlsli`, and the post-build
step copies all `.hlsl`/`.hlsli` next to the exe. Shaders compile at build time via DXC, so an HLSL
syntax error is a build error — but a *register* mismatch is not, since registers are only validated
against the root signature at draw time by the debug layer. Keep the debug layer on.

## 11. Invariant checklist

- [ ] Every render-target format appears in exactly three places, all agreeing:
      `GeometryRenderPass::Init`, `GeometryRenderPass::CreatePSO`, `MaterialTemplate::BuildPSODesc`.
- [ ] `Geometry_PS`'s `SV_Target` indices are contiguous from 0 and match the order of
      `m_RenderTargets` in `Init`.
- [ ] `GetRenderTarget`'s index for each `ResourceSlot` matches that same order.
- [ ] The demo's `gBufferTypes` / `ssrGBufferTypes` order matches the consumer shaders' register
      order, and both lists are the same length as the G-buffer block width.
- [ ] `Pack`/`Unpack` (the `*0.5+0.5` and `*2-1`) each appear exactly once, in the header.
- [ ] Every vector passed to `OctEncode` is normalised first.
- [ ] No shader declares its own reconstruction or oct maths; both include `GBufferUtils.hlsli`.
- [ ] Reconstruction uses `ScreenData`'s `InvProjectionMatrix` — the jittered one — not a
      separately derived inverse.
- [ ] `SSRRenderPass::CreatePSO`'s history table base register is still
      `m_InputResources.size()`, not a hardcoded number.

## 12. Gotchas specific to this task

- **`MaterialTemplate.cpp:109` is a silent duplicate of the geometry format list.** Nothing links
  the two. A mismatch fails at PSO creation or draw time, per material, and reads as a material bug.
- **`historyMap`'s register is data-derived.** `SSRRenderPass::CreatePSO:241` appends its table at
  `m_InputResources.size()`, so it moves whenever the G-buffer block width changes — but only the
  HLSL declaration needs your attention; the C++ tracks it automatically.
- **`RenderPass::Init` and `RebuildTransientDescriptors` walk different orders.** `Init` uses
  `m_ResourceBlockOrder` (insertion order), `RebuildTransientDescriptors` uses the global
  `OrderedInputTypes`. They happen to agree today because `CreateRenderPipeline` populates blocks in
  `OrderedInputTypes` order. If you ever add a block outside that loop — as
  `LightingRenderPass::Init:30` does for its fallback env map, safely, because `EnvironmentMap` is
  first — the two orders diverge and registers silently disagree with descriptors.
- **`input.normal` is not normalised in `Geometry_PS`.** Line 110 writes it raw today, which was
  tolerable when the consumer normalised on read. Octahedral encoding of a non-unit vector is
  garbage. Normalise at the write.
- **Reverse-Z is already in effect.** Depth clears to `0.0`, `GREATER` test, sky at `depth <=
  0.001`. Reconstruction needs no special case — but if you copy a reconstruction snippet from
  anywhere else on the internet, check whether it assumes `depth == 1` at the far plane.
- **`emissiveMap.a` carries AO.** It is easy to look at a 6-target G-buffer and think Emissive is
  a candidate for narrowing to `R11G11B10_FLOAT`. It is not, until AO has somewhere else to live.
- **The half-texel nudge is inherited, not endorsed.** §4c. Do not silently "fix" it inside this
  change.
- **Shader hot-reload only exists in Debug.** `ResourceManager::ReloadChangedShaders` is
  `#ifndef _DEBUG return false`. Iterate in Debug; verify the final result in both configs, because
  a Release build recompiles PSOs from scratch and would surface a format mismatch you had been
  hot-reloading past.

## 13. Validation

No test suite — running `DemoScene` is the instrument.

- **Per slice, A/B the frame.** Screenshot the same camera position before and after. Slices 1, 2
  and 4 must be pixel-identical or near-identical; slice 3 may differ by a shade on curved
  highlights; slice 5 is the only one where a real difference is expected, and it should be
  invisible.
- **The reconstruction diff (slice 1 only).** §4e. Do it — it is the only chance to check the maths
  against ground truth.
- **Normal round-trip.** Temporarily return `UnpackNormal(PackNormal(worldNormal)) - worldNormal`
  scaled by 100 from `Geometry_PS`. Expect black. Any structure means §6b.
- **Rotate the camera through all six axes.** Octahedral encoding has its worst error near the
  octahedron's edges; a fold bug shows as lighting flipping as a surface's normal crosses an axis.
  Point the camera straight up and straight down in particular.
- **SSR under motion.** Move along a reflective floor. SSR is the most sensitive consumer of both
  reconstruction and normals; ghosting or reflections detaching from their surfaces means the
  reconstruction disagrees with what the ray march expects.
- **Shadows.** All three kinds — spot, point, cascaded — depend on `worldPos`. Check that contact
  points are still tight against their casters; a reconstruction bias shows as a uniform shadow
  offset rather than as noise.
- **Memory.** PIX or Task Manager: G-buffer allocation should drop by ~40% at your window size.
  With `RenderScale` above 1.0 (`DX12EngineDemoApp.cpp:31` has the commented-out `1.5f`) the
  saving scales quadratically — a good way to make it visible.
- **Resize.** Drag the window edge; `RenderPass::OnResize` recreates every target from its
  `RenderTextureConfig`, so a format that is right at startup and wrong after resize means you
  changed `CreatePSO` but not `Init`.
- **Release build.** Build and run `--config Release` once at the end (§12, last bullet).

## 14. Close-out

- **Update `docs/Rendering-Architecture.md`.** §4.1's table and its "Total ≈ 52 bytes/px" figure;
  the two bullets below it about Position and normals being redundant — those are now *done*, not
  *recommended*; §14 weakness 4; §16 Tier 2 item 6. If you stopped after slice 4, say 36 B/px, not
  32.
- **Log the follow-up.** Add the quarter-texel nudge (§4c) to §16's Tier 4 list — it is a real
  correctness question and it will be forgotten otherwise.
- **Style.** `CPP-STYLE-GUIDE.md`. HLSL in this repo uses 4-space indentation and `camelCase`
  locals with `PascalCase` cbuffer fields — match `PBRShading.hlsli` and `ColorUtils.hlsli`. Comment
  the swizzle in `OctEncode` and the `[-1,1]`↔`[0,1]` mapping in `PackNormal`; those are exactly the
  non-obvious *why* §9 of the style guide asks for. Nothing else in the header needs a comment.
- **Commit.** One per slice, matching the repo's descriptive-summary style
  (`3f616ee Rendering fixes 1: ...`), or one `Rendering fixes 3: Slim the G-buffer` if you prefer
  the tier granularity.

## 15. Optional / future

- **Narrow Emissive to `R11G11B10_FLOAT`** (8 → 4 B/px, total 28). Needs AO rehoused — the spare
  `.a` of the packed Material target is the obvious candidate once it is `RGBA8`, since only three
  of its four channels would then be in use if clearcoat moves.
- **Drop `ObjectNormal` entirely.** It is used in exactly two places: a shadow-offset term
  (`PBRLightingDeferred_PS.hlsl:117`) and SSR's `SampleGeometricNormal`. Both could plausibly use
  a depth-derived geometric normal (`ddx`/`ddy` of reconstructed position), taking normals to 4
  B/px and the total to 28. Cheaper still, but it trades exact data for a derivative approximation
  that is noisy at depth discontinuities — measure before committing.
- **Fix the quarter-texel nudge.** §4c. Small, isolated, and best done with SSR screenshots in hand.
- **Split-sum BRDF LUT** (Tier 4 item 12) is the natural next shader-side change and touches the
  same file; batching them would save a round of validation.
