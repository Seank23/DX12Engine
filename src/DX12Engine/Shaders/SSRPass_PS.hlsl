#include "Common/ScreenData.hlsli"

cbuffer TemporalBuffer : register(b1)
{
    float4x4 PrevViewMatrix;
    float4x4 PrevProjectionMatrix;
    uint FrameIndex;
    float3 TemporalPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_TARGET0; // composite output consumed by the final present pass
    float4 history : SV_TARGET1; // written into the ping-pong history buffer
    float reactive : SV_TARGET2; // reactive mask consumed by TAA
};

// External textures
TextureCube environmentMap : register(t0);
// G-buffer inputs
Texture2D albedoMap : register(t1);
Texture2D normalsMap : register(t2);
Texture2D materialMap : register(t3);
Texture2D emissiveMap : register(t4);
Texture2D depthMap : register(t5);
Texture2D pipelineOutputMap : register(t6);
// Temporal history (read-only, previous frame result)
Texture2D historyMap : register(t7);

SamplerState samp : register(s0);

#include "Common/ColorUtils.hlsli"
#include "Common/GBufferUtils.hlsli"
#include "Lighting/PBRShading.hlsli"

// ---------------------------------------------------------------------------
// Interleaved gradient noise – spatially varied, low-discrepancy, no obvious
// hash bands. Much better perceptual quality than a raw hash function.
// ---------------------------------------------------------------------------
float InterleavedGradientNoise(float2 screenPos, uint frame)
{
    screenPos += float(frame) * 5.588238f;
    return frac(52.9829189f * frac(dot(screenPos, float2(0.06711056f, 0.00583715f))));
}

void ComputePositionAndReflection(
    float2 texCoord, float3 normalVS, float roughnessBias,
    out float3 positionTS, out float3 reflectedDirTS,
    out float3 positionVS, out float3 reflectedDirVS,
    out float maxDistance)
{
    float sampledDepth = depthMap.Sample(samp, texCoord).r;

    float4 samplePosCS = float4(texCoord * 2.0 - 1.0, sampledDepth, 1.0);
    samplePosCS.xy += 0.5 / ScreenSize;
    samplePosCS.y *= -1.0;

    float4 samplePosVS4 = mul(InvProjectionMatrix, samplePosCS);
    samplePosVS4 /= samplePosVS4.w;

    float3 sampleDirVS = normalize(samplePosVS4.xyz);
    float3 reflDir = reflect(sampleDirVS, normalVS);

    // Apply roughness-based cone jitter in view space.
    // Scale jitter by roughness² for perceptually linear spread width.
    float coneSpread = roughnessBias * roughnessBias * 0.4;
    float3 jitterDir = normalize(reflDir + float3(roughnessBias, roughnessBias * 0.7, roughnessBias * 0.3) * coneSpread);
    reflectedDirVS = jitterDir;

    float4 reflectedRayEndVS = samplePosVS4 + float4(jitterDir, 0.0);
    float4 reflectedRayEndCS = mul(ProjectionMatrix, float4(reflectedRayEndVS.xyz, 1.0));
    reflectedRayEndCS /= reflectedRayEndCS.w;

    float3 reflectedDirCS = normalize(reflectedRayEndCS.xyz - samplePosCS.xyz);

    // Convert start position and direction to texture (UV+depth) space
    samplePosCS.xy = samplePosCS.xy * float2(0.5, -0.5) + 0.5;
    reflectedDirCS.xy *= float2(0.5, -0.5);
    reflectedDirCS *= max(0.1, -samplePosCS.z);

    positionTS = samplePosCS.xyz;
    reflectedDirTS = reflectedDirCS;
    positionVS = samplePosVS4.xyz;

    // Clamp march distance to the first screen boundary hit
    maxDistance = reflectedDirTS.x >= 0.0
        ? (1.0 - positionTS.x) / reflectedDirTS.x
        : -positionTS.x / reflectedDirTS.x;
    maxDistance = min(maxDistance, reflectedDirTS.y < 0.0
        ? -positionTS.y / reflectedDirTS.y
        : (1.0 - positionTS.y) / reflectedDirTS.y);
    maxDistance = min(maxDistance, reflectedDirTS.z < 0.0
        ? -positionTS.z / reflectedDirTS.z
        : (1.0 - positionTS.z) / reflectedDirTS.z);
}

// ---------------------------------------------------------------------------
// Linear ray march with a coarse pass followed by binary-search refinement.
// Returns a confidence value in [0,1] rather than a bool so that the caller
// can blend rather than hard-switch.
// ---------------------------------------------------------------------------
float FindIntersection(
    float3 samplePosTS, float3 reflectedDirTS,
    float3 reflectedDirVS, float3 normalVS,
    float roughness,
    float maxTraceDistance, out float3 intersectionPosTS)
{
    // Backface check: if the reflection direction points into the same
    // hemisphere as the surface normal in view space, the ray would hit the
    // back side of geometry – skip it entirely.
    if (dot(reflectedDirVS, normalVS) < 0.0)
    {
        intersectionPosTS = samplePosTS;
        return 0.0;
    }

    float3 reflectionEndTS = samplePosTS + reflectedDirTS * maxTraceDistance;

    int2 sampleScreenPos = int2(samplePosTS.xy * ScreenSize);
    int2 endPosScreenPos = int2(reflectionEndTS.xy * ScreenSize);
    int2 dPos2 = endPosScreenPos - sampleScreenPos;
    int maxDist = max(abs(dPos2.x), abs(dPos2.y));

    const int maxSteps = 200;
    float stridePixels = lerp(1.0, 2.0, saturate(roughness * 2.0));
    int traceSteps = max(1, min(maxSteps, (int) ceil(float(maxDist) / stridePixels)));
    float3 dPos = (reflectionEndTS.xyz - samplePosTS.xyz) / float(traceSteps);

    // Smooth surfaces need tighter hit precision to preserve thin reflected detail.
    float baseThickness = lerp(0.004, 0.008, roughness);

    float3 rayPos = samplePosTS + dPos; // skip the source texel

    int hitIndex = -1;
    for (int i = 1; i < traceSteps; i++)
    {
        float sceneDepth = depthMap.Sample(samp, rayPos.xy).r;
        // Reverse-Z: larger depth == closer, so the ray is behind the stored
        // surface when its depth is smaller than the scene depth.
        float thickness = sceneDepth - rayPos.z;
        float adaptiveThickness = baseThickness * (1.0 + abs(rayPos.z) * 4.0);
        if (thickness > 0.0 && thickness < adaptiveThickness)
        {
            hitIndex = i;
            break;
        }
        rayPos += dPos;
    }

    if (hitIndex < 0)
    {
        intersectionPosTS = rayPos;
        return 0.0;
    }

    // Binary search refinement: narrow down the hit between the previous and
    // current step to reduce edge-of-object ray tunnelling artefacts.
    float3 lo = rayPos - dPos;
    float3 hi = rayPos;
    [unroll(6)]
    for (int b = 0; b < 6; b++)
    {
        float3 mid = (lo + hi) * 0.5;
        float sceneD = depthMap.Sample(samp, mid.xy).r;
        // Reverse-Z: behind the surface means a smaller depth than the scene.
        float thickness = sceneD - mid.z;
        float adaptiveT = baseThickness * (1.0 + abs(mid.z) * 4.0);
        if (thickness > 0.0 && thickness < adaptiveT)
            hi = mid;
        else
            lo = mid;
    }
    intersectionPosTS = (lo + hi) * 0.5;

    // Fade confidence near screen edges so SSR blends back to env map smoothly
    float2 hitUV = intersectionPosTS.xy;
    float2 edgeDist = smoothstep(0.0, 0.03, hitUV) * (1.0 - smoothstep(0.97, 1.0, hitUV));
    float edgeFade = lerp(0.20, 1.0, edgeDist.x * edgeDist.y);

    // Fade confidence with ray travel distance (long rays are less reliable)
    float rayTravel = float(hitIndex) / float(max(traceSteps, 1));
    float distFade = 1.0 - smoothstep(0.75, 1.0, rayTravel);
    distFade = lerp(0.70, 1.0, distFade);

    return saturate(edgeFade * distFade);
}

// ---------------------------------------------------------------------------
// Reproject a world-space position into the previous frame's UV coordinates
// for temporal history lookup.
// ---------------------------------------------------------------------------
float2 ReprojectUV(float3 positionWS)
{
    float4 prevClip = mul(PrevProjectionMatrix, mul(PrevViewMatrix, float4(positionWS, 1.0)));
    prevClip /= prevClip.w;
    // Convert NDC [-1,1] to UV [0,1], accounting for DX12 y-flip
    float2 prevUV = prevClip.xy * float2(0.5, -0.5) + 0.5;
    return prevUV;
}

// ---------------------------------------------------------------------------
// Sample the irradiance cubemap using the world-space reflection direction as
// a fallback when the screen-space ray misses or has low confidence.
// ---------------------------------------------------------------------------
float3 SampleIrradianceFallback(float3 normalWS, float3 viewDirWS, float roughness)
{
    float3 reflWS = reflect(viewDirWS, normalWS);
    float perceptualRoughness = roughness * roughness;
    // Sample the specular environment map (not the diffuse irradiance map) at a
    // mip driven by perceptual roughness so rough misses blend to a correctly
    // blurred specular environment rather than a flat diffuse average.
    return sRGBToLinear(environmentMap.SampleLevel(samp, reflWS, perceptualRoughness * 12.0).rgb);
}

// ---------------------------------------------------------------------------
// Compose SSR hit color, irradiance fallback and Fresnel weighting.
// confidence == 1 → full SSR, confidence == 0 → full irradiance fallback.
// ---------------------------------------------------------------------------
float3 ComputeReflectionWeight(
    float3 normalWS,
    float3 surfaceToCameraWS,
    float3 albedo,
    float roughness,
    float metallic,
    float clearcoat,
    float ao)
{
    float NdotV = saturate(dot(normalize(normalWS), normalize(surfaceToCameraWS)));
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    float specularWeight = (1.0 - roughness) * (1.0 - roughness);
    float specOcc = SpecularOcclusion(NdotV, ao, roughness);
    float clearcoatFresnel = FresnelSchlick(NdotV, float3(0.04, 0.04, 0.04)).r;
    float clearcoatWeight = clearcoat * clearcoatFresnel * NdotV * specOcc;
    return kS * specularWeight * specOcc + clearcoatWeight.xxx;
}

float EstimateFallbackVisibility(float3 sceneColor, float3 emissive, float3 fallbackContribution, float ao)
{
    float sceneLuma = Luma(max(sceneColor - emissive, 0.0));
    float fallbackLuma = max(Luma(fallbackContribution), 1e-3);
    float lumaRatio = sceneLuma / (fallbackLuma + 0.1);
    float visibility = lerp(0.35, 1.0, saturate(lumaRatio));
    visibility *= lerp(0.85, 1.0, ao);
    return saturate(visibility);
}

float SurfaceEdgeFactor(float2 texCoord, float3 normalWS)
{
    float2 texel = 1.0 / ScreenSize;
    float depthCenter = LoadMap(texCoord, depthMap).r;
    float depthX = abs(LoadMap(texCoord + float2(texel.x, 0.0), depthMap).r - depthCenter);
    float depthY = abs(LoadMap(texCoord + float2(0.0, texel.y), depthMap).r - depthCenter);

    float3 normalX = LoadWorldNormal(texCoord + float2(texel.x, 0.0), normalsMap);
    float3 normalY = LoadWorldNormal(texCoord + float2(0.0, texel.y), normalsMap);
    float normalDelta = (1.0 - saturate(dot(normalWS, normalX))) + (1.0 - saturate(dot(normalWS, normalY)));

    float depthEdge = saturate((depthX + depthY) * 48.0);
    float normalEdge = saturate(normalDelta * 3.0);
    return saturate(max(depthEdge, normalEdge));
}

float DepthEdgeFactor(float2 texCoord)
{
    float2 texel = 1.0 / ScreenSize;
    float depthCenter = LoadMap(texCoord, depthMap).r;
    float depthX = abs(LoadMap(texCoord + float2(texel.x, 0.0), depthMap).r - depthCenter);
    float depthY = abs(LoadMap(texCoord + float2(0.0, texel.y), depthMap).r - depthCenter);
    return saturate((depthX + depthY) * 56.0);
}

float HitSurfaceReflectivity(float2 uv)
{
    float4 hitMaterial = materialMap.SampleLevel(samp, uv, 0);
    float hitRoughness = saturate(hitMaterial.r);
    float hitMetallic = saturate(hitMaterial.g);
    float hitClearcoat = saturate(hitMaterial.b);
    float hitClearcoatRoughness = saturate(hitMaterial.a);
    float hitGloss = 1.0 - hitRoughness;
    float hitClearcoatGloss = hitClearcoat * (1.0 - hitClearcoatRoughness);
    return saturate(max(hitGloss * hitGloss, max(hitMetallic, hitClearcoatGloss)));
}

float HitValidation(float2 hitUV, float3 reflectedDirVS, float3 receiverGeomNormal)
{
    float4 hitPacked = LoadMap(hitUV, normalsMap);
    float3 hitNormalWS = UnpackNormal(hitPacked.xy);
    float3 hitGeomNormal = UnpackNormal(hitPacked.zw);
    float3 hitNormalVS = normalize(mul(ViewMatrix, float4(hitNormalWS, 0.0)).xyz);
    float facing = saturate(-dot(hitNormalVS, normalize(reflectedDirVS)));
    float hitEdge = DepthEdgeFactor(hitUV);
    float normalMismatch = 1.0 - abs(dot(receiverGeomNormal, hitGeomNormal));
    float receiverVertical = 1.0 - saturate(abs(receiverGeomNormal.y));
    float hitHorizontal = saturate(abs(hitGeomNormal.y));
    float facingWeight = lerp(0.70, 1.0, facing);
    float edgeWeight = lerp(1.0, 0.70, hitEdge);
    float crossSurfaceWeight = lerp(1.0, 0.72, saturate(normalMismatch * receiverVertical * hitHorizontal));
    return saturate(facingWeight * edgeWeight * crossSurfaceWeight);
}

float EstimateRecursiveHotspotSuppression(
    float2 hitUV,
    float3 receiverGeomNormal,
    float fallbackVisibility,
    float hotspotRatio)
{
    float3 hitGeomNormal = SampleGeometricNormal(hitUV, normalsMap);
    float hitReflectivity = HitSurfaceReflectivity(hitUV);
    float normalMismatch = 1.0 - abs(dot(receiverGeomNormal, hitGeomNormal));
    float receiverVertical = 1.0 - saturate(abs(receiverGeomNormal.y));
    float hitHorizontal = saturate(abs(hitGeomNormal.y));
    float crossSurface = saturate(max(normalMismatch * 0.8, receiverVertical * hitHorizontal));
    float receiverShaded = 1.0 - fallbackVisibility;
    float hotspot = smoothstep(1.6, 5.0, hotspotRatio);
    return saturate(receiverShaded * hitReflectivity * crossSurface * hotspot);
}

float3 SampleFilteredSSRHit(float2 hitUV, float roughness)
{
    float3 center = pipelineOutputMap.SampleLevel(samp, hitUV, 0).rgb;
    float filterAmount = saturate(roughness * 1.35);
    if (filterAmount <= 0.01)
    {
        return center;
    }

    float2 texel = 1.0 / ScreenSize;
    float2 axis = texel * lerp(0.75, 2.5, filterAmount);

    float3 neighborhood = center * 4.0;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV + float2(axis.x, 0.0), 0).rgb;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV - float2(axis.x, 0.0), 0).rgb;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV + float2(0.0, axis.y), 0).rgb;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV - float2(0.0, axis.y), 0).rgb;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV + axis, 0).rgb * 0.5;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV - axis, 0).rgb * 0.5;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV + float2(axis.x, -axis.y), 0).rgb * 0.5;
    neighborhood += pipelineOutputMap.SampleLevel(samp, hitUV + float2(-axis.x, axis.y), 0).rgb * 0.5;
    neighborhood /= 8.0;

    return lerp(center, neighborhood, filterAmount * 0.7);
}

float4 ComputeReflectedColor(
    float confidence, float3 intersectionPosTS,
    float3 sceneColor, float3 emissive,
    float metallic,
    float3 normalWS, float3 viewDirWS,
    float roughness,
    float3 albedo, float clearcoat, float ao,
    float3 surfaceToCameraWS,
    float3 receiverGeomNormal)
{
    float3 reflectionWeight = ComputeReflectionWeight(normalWS, surfaceToCameraWS, albedo, roughness, metallic, clearcoat, ao);
    float3 ssrSample = SampleFilteredSSRHit(intersectionPosTS.xy, roughness);
    float3 fallbackSample = SampleIrradianceFallback(normalWS, viewDirWS, roughness);
    float3 ssrContribution = ssrSample * reflectionWeight;
    float3 fallbackContribution = fallbackSample * reflectionWeight;
    float fallbackVisibility = EstimateFallbackVisibility(sceneColor, emissive, fallbackContribution, ao);
    float fallbackRetention = lerp(0.12, 0.30, 1.0 - fallbackVisibility);
    fallbackRetention = max(fallbackRetention, roughness * 0.08);
    float fallbackReplacement = 1.0 - fallbackRetention;

    // The deferred lighting pass already contributes the environment fallback.
    // In shadowed regions the lighting pass does not necessarily contribute the
    // full fallback environment term, so scale the subtraction heuristically.
    float3 reflectionDelta = ssrContribution - fallbackContribution * fallbackVisibility * fallbackReplacement;

    float3 positiveDelta = max(reflectionDelta, 0.0);
    float3 negativeDelta = min(reflectionDelta, 0.0);
    float hotspotRatio = Luma(ssrContribution) / max(Luma(fallbackContribution) + 0.05, 0.1);
    float hotspotCompression = smoothstep(2.0, 8.0, hotspotRatio) * (1.0 - fallbackVisibility);
    float recursiveHotspotSuppression = EstimateRecursiveHotspotSuppression(
        intersectionPosTS.xy,
        receiverGeomNormal,
        fallbackVisibility,
        hotspotRatio);
    positiveDelta *= lerp(1.0, 0.55, hotspotCompression);
    positiveDelta *= lerp(1.0, 0.35, recursiveHotspotSuppression);
    float3 maxPositiveDelta = fallbackContribution * lerp(6.0, 2.5, recursiveHotspotSuppression) + float3(0.05, 0.05, 0.05);
    positiveDelta = min(positiveDelta, maxPositiveDelta);

    confidence *= lerp(1.0, 0.75, recursiveHotspotSuppression);
    reflectionDelta = (positiveDelta + negativeDelta) * confidence;
    return float4(reflectionDelta, confidence);
}

float ComputeReactiveMask(
    float2 texCoord,
    float3 sceneColor,
    float3 emissive,
    float roughness,
    float metallic,
    float clearcoat,
    float3 normalWS,
    float3 surfaceToCameraWS,
    float confidence,
    float previousConfidence,
    float3 currentReflection,
    bool offScreen,
    bool disoccluded)
{
    float edgeFactor = SurfaceEdgeFactor(texCoord, normalWS);
    float3 litColor = max(sceneColor - emissive, 0.0);
    float emissiveReactive = saturate((Max3(emissive) - 0.75) * 0.35);

    float smoothSurface = saturate(1.0 - roughness);
    float NdotV = saturate(dot(normalize(normalWS), normalize(surfaceToCameraWS)));
    float grazing = pow(1.0 - NdotV, 4.0);
    float reflectivity = saturate(lerp(0.08, 1.0, metallic) + clearcoat * 0.6 + grazing * 0.35);
    float specularSignal = Max3(litColor) * smoothSurface * reflectivity;
    float reflectionSignal = Max3(currentReflection) * smoothSurface;
    float specularReactive = saturate((max(specularSignal, reflectionSignal) - 1.0) * 0.35);
    specularReactive *= lerp(0.75, 1.15, edgeFactor);

    float confidenceReactive = saturate(abs(confidence - previousConfidence) * 2.5);
    if (offScreen || disoccluded)
    {
        confidenceReactive = max(confidenceReactive, confidence);
    }

    return saturate(max(max(specularReactive, emissiveReactive), confidenceReactive));
}

PSOutput main(PSInput input)
{
    PSOutput output;

    float2 texCoord = input.texCoord;
    float3 sceneColor = pipelineOutputMap.Sample(samp, texCoord).rgb;
    float4 material = materialMap.Sample(samp, texCoord);
    float roughness = material.r;
    float metallic = material.g;
    float clearcoat = material.b;
    float3 albedo = albedoMap.Sample(samp, texCoord).rgb;
    float4 emissiveAo = emissiveMap.Sample(samp, texCoord);
    float3 emissive = emissiveAo.rgb;
    float ao = saturate(emissiveAo.a);
    float depth = depthMap.Sample(samp, texCoord).r;

    if (depth <= 0.001) // Reverse-Z: the far plane / sky sits at depth 0.0
    {
        output.color = float4(sceneColor, 1.0);
        output.history = float4(sceneColor, 0.0);
        output.reactive = 0.0;
        return output;
    }

    float4 packedNormals = LoadMap(texCoord, normalsMap);
    float3 normalWS = UnpackNormal(packedNormals.xy);
    float3 receiverGeomNormal = UnpackNormal(packedNormals.zw);
    float3 positionWS = ReconstructWorldPos(texCoord, depth);
    float3 incidentDirWS = normalize(positionWS - CameraPosition.xyz);
    float3 surfaceToCameraWS = -incidentDirWS;

    if (max(metallic, clearcoat) < 0.01)
    {
        output.reactive = ComputeReactiveMask(
            texCoord,
            sceneColor,
            emissive,
            roughness,
            metallic,
            clearcoat,
            normalWS,
            surfaceToCameraWS,
            0.0,
            0.0,
            float3(0.0, 0.0, 0.0),
            true,
            false);
        output.color = float4(sceneColor, 1.0);
        output.history = float4(sceneColor, 0.0);
        return output;
    }

    float3 normalVS = normalize(mul(ViewMatrix, float4(normalWS, 0.0)).xyz);

    // Reconstruct world-space view direction for the fallback sampler
    float3 viewDirWS = incidentDirWS;

    // Interleaved gradient noise drives roughness-cone jitter – varies each
    // frame for temporal accumulation to fill in the sample cone over time.
    float noise = InterleavedGradientNoise(input.position.xy, FrameIndex);
    float roughnessBias = (noise * 2.0 - 1.0) * roughness;

    float3 positionTS, reflectedDirTS, positionVS, reflectedDirVS;
    float maxDistance;
    ComputePositionAndReflection(
        texCoord, normalVS, roughnessBias,
        positionTS, reflectedDirTS, positionVS, reflectedDirVS, maxDistance);

    float3 intersectionPosTS;
    float confidence = FindIntersection(
        positionTS, reflectedDirTS, reflectedDirVS, normalVS,
        roughness, maxDistance, intersectionPosTS);

    if (confidence > 0.0)
    {
        confidence *= HitValidation(intersectionPosTS.xy, reflectedDirVS, receiverGeomNormal);
    }

    float4 currentSSR = ComputeReflectedColor(
        confidence, intersectionPosTS,
        sceneColor, emissive,
        metallic,
        normalWS, viewDirWS, roughness,
        albedo, clearcoat, ao, surfaceToCameraWS, receiverGeomNormal);

    // ------------------------------------------------------------------
    // Temporal accumulation
    // Reproject this pixel into the previous frame and blend with history.
    // Alpha channel of currentSSR carries confidence for weight clamping.
    // ------------------------------------------------------------------
    float2 prevUV = ReprojectUV(positionWS);

    bool offScreen = any(prevUV < 0.0) || any(prevUV > 1.0);
    float2 clampedPrevUV = saturate(prevUV);
    float4 historySamplePacked = historyMap.Sample(samp, clampedPrevUV);
    float3 historySample = historySamplePacked.rgb;
    float previousConfidence = historySamplePacked.a;

    // Neighbourhood clamping: clamp history into the local colour bounding box
    // of the current SSR sample to suppress ghosting from disoccluded regions.
    // Use a tighter clamp (±5% + small epsilon) to prevent car-body colours
    // from bleeding into floor reflection history at silhouette edges.
    float3 clampExtent = abs(currentSSR.rgb) * 0.75 + float3(0.03, 0.03, 0.03);
    float3 colorMin = currentSSR.rgb - clampExtent;
    float3 colorMax = currentSSR.rgb + clampExtent;
    historySample = clamp(historySample, colorMin, colorMax);

    // Depth disocclusion: compare the depth at the reprojected UV against the
    // depth reconstructed from the current pixel's world-space position.
    // If they differ significantly the history sample belongs to a different
    // surface (e.g. car body vs floor) and should be discarded entirely.
    float historyDepth = depthMap.Sample(samp, clampedPrevUV).r;
    float currentDepth = depthMap.Sample(samp, texCoord).r;
    bool disoccluded = abs(historyDepth - currentDepth) > 0.005;

    // Blend weight: high confidence → blend more of the current frame for fast
    // convergence. Low confidence → lean on history more to smooth out misses.
    // Discard history entirely on the first frame, when the reprojected UV is
    // outside the screen, or when a depth disocclusion is detected.
    float blendAlpha = (FrameIndex == 0 || offScreen || disoccluded) ? 1.0 : lerp(0.12, 0.45, confidence);
    float3 accumulatedSSR = lerp(historySample, currentSSR.rgb, blendAlpha);
    float accumulatedConfidence = (FrameIndex == 0 || offScreen || disoccluded) ? confidence : lerp(previousConfidence, confidence, blendAlpha);

    output.reactive = ComputeReactiveMask(
        texCoord,
        sceneColor,
        emissive,
        roughness,
        metallic,
        clearcoat,
        normalWS,
        surfaceToCameraWS,
        confidence,
        previousConfidence,
        abs(currentSSR.rgb),
        offScreen,
        disoccluded);

    float3 finalColor = sceneColor + accumulatedSSR;
    output.color = float4(finalColor, 1.0);
    output.history = float4(accumulatedSSR, accumulatedConfidence);
    return output;
}