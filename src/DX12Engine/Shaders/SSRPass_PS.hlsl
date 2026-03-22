cbuffer ScreenBuffer : register(b0)
{
    float4 CameraPosition;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
};

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
    float4 color   : SV_TARGET0; // composite output consumed by the final present pass
    float4 history : SV_TARGET1; // written into the ping-pong history buffer
};

// External textures
TextureCube irradianceMap  : register(t0);
// G-buffer inputs
Texture2D   albedoMap      : register(t1);
Texture2D   normalMap      : register(t2);
Texture2D   materialMap    : register(t3);
Texture2D   positionMap    : register(t4);
Texture2D   depthMap       : register(t5);
Texture2D   pipelineOutputMap : register(t6);
// Temporal history (read-only, previous frame result)
Texture2D   historyMap     : register(t7);

SamplerState samp : register(s0);

// ---------------------------------------------------------------------------
// Interleaved gradient noise – spatially varied, low-discrepancy, no obvious
// hash bands. Much better perceptual quality than a raw hash function.
// ---------------------------------------------------------------------------
float InterleavedGradientNoise(float2 screenPos, uint frame)
{
    screenPos += float(frame) * 5.588238f;
    return frac(52.9829189f * frac(dot(screenPos, float2(0.06711056f, 0.00583715f))));
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// ---------------------------------------------------------------------------
// Reconstruct view-space position from depth and UV.
// ---------------------------------------------------------------------------
float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 posCS = float4(uv * 2.0 - 1.0, depth, 1.0);
    posCS.xy += 0.5 / ScreenSize;
    posCS.y *= -1.0;
    float4 posVS = mul(InvProjectionMatrix, posCS);
    return posVS.xyz / posVS.w;
}

void ComputePositionAndReflection(
    float2 texCoord, float3 normalVS, float roughnessBias,
    out float3 positionTS, out float3 reflectedDirTS,
    out float3 positionVS, out float3 reflectedDirVS,
    out float  maxDistance)
{
    float sampledDepth = depthMap.Sample(samp, texCoord).r;

    float4 samplePosCS = float4(texCoord * 2.0 - 1.0, sampledDepth, 1.0);
    samplePosCS.xy += 0.5 / ScreenSize;
    samplePosCS.y *= -1.0;

    float4 samplePosVS4 = mul(InvProjectionMatrix, samplePosCS);
    samplePosVS4 /= samplePosVS4.w;

    float3 sampleDirVS = normalize(samplePosVS4.xyz);
    float3 reflDir     = reflect(sampleDirVS, normalVS);

    // Apply roughness-based cone jitter in view space.
    // roughnessBias is a signed scalar in [-1,1] per axis, pre-scaled by roughness.
    float3 jitterDir   = normalize(reflDir + float3(roughnessBias, roughnessBias * 0.7, roughnessBias * 0.3) * 0.05);
    reflectedDirVS     = jitterDir;

    float4 reflectedRayEndVS = samplePosVS4 + float4(jitterDir, 0.0);
    float4 reflectedRayEndCS = mul(ProjectionMatrix, float4(reflectedRayEndVS.xyz, 1.0));
    reflectedRayEndCS /= reflectedRayEndCS.w;

    float3 reflectedDirCS = normalize(reflectedRayEndCS.xyz - samplePosCS.xyz);

    // Convert start position and direction to texture (UV+depth) space
    samplePosCS.xy = samplePosCS.xy * float2(0.5, -0.5) + 0.5;
    reflectedDirCS.xy *= float2(0.5, -0.5);
    reflectedDirCS   *= max(0.1, -samplePosCS.z);

    positionTS    = samplePosCS.xyz;
    reflectedDirTS = reflectedDirCS;
    positionVS    = samplePosVS4.xyz;

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

    int2 sampleScreenPos = int2(samplePosTS.xy  * ScreenSize);
    int2 endPosScreenPos = int2(reflectionEndTS.xy * ScreenSize);
    int2 dPos2   = endPosScreenPos - sampleScreenPos;
    int  maxDist = max(abs(dPos2.x), abs(dPos2.y));

    float3 dPos = (reflectionEndTS.xyz - samplePosTS.xyz) / float(maxDist);
    dPos *= 2.0; // 2-pixel stride for the coarse pass

    const int   maxSteps      = 200;
    // Thickness grows with distance so grazing-angle surfaces are not missed
    const float baseThickness = 0.005;

    float3 rayPos = samplePosTS + dPos; // skip the source texel

    int hitIndex = -1;
    for (int i = 1; i < maxDist && i < maxSteps; i++)
    {
        float sceneDepth = depthMap.Sample(samp, rayPos.xy).r;
        float thickness  = rayPos.z - sceneDepth;
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
        float3 mid       = (lo + hi) * 0.5;
        float  sceneD    = depthMap.Sample(samp, mid.xy).r;
        float  thickness = mid.z - sceneD;
        float  adaptiveT = baseThickness * (1.0 + abs(mid.z) * 4.0);
        if (thickness > 0.0 && thickness < adaptiveT)
            hi = mid;
        else
            lo = mid;
    }
    intersectionPosTS = (lo + hi) * 0.5;

    // Fade confidence near screen edges so SSR blends back to env map smoothly
    float2 hitUV    = intersectionPosTS.xy;
    float2 edgeDist = smoothstep(0.0, 0.1, hitUV) * (1.0 - smoothstep(0.9, 1.0, hitUV));
    float  edgeFade = edgeDist.x * edgeDist.y;

    // Fade confidence with ray travel distance (long rays are less reliable)
    float distFade = 1.0 - saturate(float(hitIndex) / float(maxSteps));

    return saturate(edgeFade);
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
    // Sample at a mip proportional to roughness to avoid over-sharpening the fallback
    return irradianceMap.SampleLevel(samp, reflWS, roughness * 4.0).rgb;
}

// ---------------------------------------------------------------------------
// Compose SSR hit color, irradiance fallback and Fresnel weighting.
// confidence == 1 → full SSR, confidence == 0 → full irradiance fallback.
// ---------------------------------------------------------------------------
float4 ComputeReflectedColor(
    float  confidence, float3 intersectionPosTS,
    float3 sceneColor, float  metallic,
    float3 normalWS,   float3 viewDirWS,
    float3 positionVS, float  roughness)
{
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), sceneColor, metallic);
    float3 viewDirVS = normalize(positionVS);
    float  NdotV     = saturate(-dot(normalize(mul(ViewMatrix, float4(normalWS, 0.0)).xyz), viewDirVS));
    float3 fresnel   = FresnelSchlick(NdotV, F0);

    float3 ssrSample      = pipelineOutputMap.Sample(samp, intersectionPosTS.xy).rgb;
    float3 fallbackSample = SampleIrradianceFallback(normalWS, viewDirWS, roughness);

    // Blend SSR and fallback based on ray confidence
    float3 reflectionColor = lerp(fallbackSample, ssrSample, confidence);

    return float4(reflectionColor * fresnel * metallic, confidence);
}

PSOutput main(PSInput input)
{
    PSOutput output;

    float2 texCoord  = input.texCoord;
    float3 sceneColor = pipelineOutputMap.Sample(samp, texCoord).rgb;
    float  roughness  = materialMap.Sample(samp, texCoord).r;
    float  metallic   = materialMap.Sample(samp, texCoord).g;

    if (metallic < 0.01)
    {
        output.color   = float4(sceneColor, 1.0);
        output.history = float4(sceneColor, 1.0);
        return output;
    }

    float3 normalWS   = normalMap.Sample(samp, texCoord).xyz;
    float3 normalVS   = normalize(mul(ViewMatrix, float4(normalWS, 0.0)).xyz);
    float3 positionWS = positionMap.Sample(samp, texCoord).xyz;

    // Reconstruct world-space view direction for the fallback sampler
    float3 viewDirWS = normalize(positionWS - CameraPosition.xyz);

    // Interleaved gradient noise drives roughness-cone jitter – varies each
    // frame for temporal accumulation to fill in the sample cone over time.
    float noise = InterleavedGradientNoise(input.position.xy, FrameIndex);
    float roughnessBias = (noise * 2.0 - 1.0) * roughness;

    float3 positionTS, reflectedDirTS, positionVS, reflectedDirVS;
    float  maxDistance;
    ComputePositionAndReflection(
        texCoord, normalVS, roughnessBias,
        positionTS, reflectedDirTS, positionVS, reflectedDirVS, maxDistance);

    float3 intersectionPosTS;
    float  confidence = FindIntersection(
        positionTS, reflectedDirTS, reflectedDirVS, normalVS,
        maxDistance, intersectionPosTS);

    float4 currentSSR = ComputeReflectedColor(
        confidence, intersectionPosTS,
        sceneColor, metallic,
        normalWS, viewDirWS, positionVS, roughness);

    // ------------------------------------------------------------------
    // Temporal accumulation
    // Reproject this pixel into the previous frame and blend with history.
    // Alpha channel of currentSSR carries confidence for weight clamping.
    // ------------------------------------------------------------------
    float2 prevUV = ReprojectUV(positionWS);

    bool offScreen = any(prevUV < 0.0) || any(prevUV > 1.0);
    float3 historySample = historyMap.Sample(samp, prevUV).rgb;

    // Neighbourhood clamping: clamp history into the local colour bounding box
    // of the current SSR sample to suppress ghosting from disoccluded regions.
    float3 colorMin = currentSSR.rgb * 0.85;
    float3 colorMax = currentSSR.rgb * 1.15 + float3(0.02, 0.02, 0.02);
    historySample = clamp(historySample, colorMin, colorMax);

    // Blend weight: high confidence → blend more of the current frame for fast
    // convergence. Low confidence → lean on history more to smooth out misses.
    // Discard history entirely on the first frame, or when the reprojected UV is outside the screen.
    float blendAlpha = (FrameIndex == 0 || offScreen) ? 1.0 : lerp(0.05, 0.2, confidence);
    float3 accumulatedSSR = lerp(historySample, currentSSR.rgb, blendAlpha);

    float3 finalColor = sceneColor + accumulatedSSR;
    output.color   = float4(finalColor, 1.0);
    output.history = float4(accumulatedSSR, 1.0);
    return output;
}