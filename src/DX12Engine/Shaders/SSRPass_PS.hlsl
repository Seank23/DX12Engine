cbuffer LightingPassBuffer : register(b0)
{
    float4 CameraPosition;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D materialMap : register(t2);
Texture2D positionMap : register(t3);
Texture2D depthMap : register(t4);
Texture2D pipelineOutputMap : register(t5);
SamplerState samp : register(s0);

float3 Hash(float3 a)
{
    a = frac(a * float3(0.8, 0.8, 0.8));
    a += dot(a, a.yxz + 19.19);
    return frac((a.xxy + a.yxx) * a.zyx);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void ComputePositionAndReflection(float2 texCoord, float3 normalVS, float3 jitter, float roughness, out float3 positionTS, out float3 reflectedDirTS, out float3 positionVS, out float maxDistance)
{
    float sampledDepth = depthMap.Sample(samp, texCoord).r;
    float4 samplePosCS = float4(texCoord * 2.0 - 1.0, sampledDepth, 1.0);
    samplePosCS.xy += 0.5 / ScreenSize;
    samplePosCS.y *= -1;
    
    float4 samplePosVS = mul(InvProjectionMatrix, samplePosCS);
    samplePosVS /= samplePosVS.w;
    float3 sampleDirVS = normalize(samplePosVS.xyz);
    float4 reflectedDirVS = float4(reflect(sampleDirVS.xyz, normalVS.xyz), 0.0);
    reflectedDirVS += float4(jitter * roughness, 1.0);
    
    float4 reflectedRayEndVS = samplePosVS + reflectedDirVS;
    
    float4 reflectedRayEndCS = mul(ProjectionMatrix, float4(reflectedRayEndVS.xyz, 1.0));
    reflectedRayEndCS /= reflectedRayEndCS.w;
    float3 reflectedDirCS = normalize(reflectedRayEndCS.xyz - samplePosCS.xyz);
    
    samplePosCS.xy = samplePosCS.xy * float2(0.5, -0.5) + 0.5;
    reflectedDirCS.xy *= float2(0.5, -0.5);
    reflectedDirCS *= max(0.1, -samplePosCS.z);
    
    positionTS = samplePosCS.xyz;
    reflectedDirTS = reflectedDirCS;
    positionVS = samplePosVS.xyz;
    
    maxDistance = reflectedDirTS.x > 0.0 ? (1.0 - positionTS.x) / reflectedDirTS.x : -positionTS.x / reflectedDirTS.x;
    maxDistance = min(maxDistance, reflectedDirTS.y < 0.0 ? (-positionTS.y / reflectedDirTS.y) : (1.0 - positionTS.y) / reflectedDirTS.y);
    maxDistance = min(maxDistance, reflectedDirTS.z < 0.0 ? (-positionTS.z / reflectedDirTS.z) : (1.0 - positionTS.z) / reflectedDirTS.z);
}

bool FindIntersection(float3 samplePosTS, float3 reflectedDirTS, float maxTraceDistance, out float3 intersectionPosTS)
{
    float3 reflectionEndTS = samplePosTS + reflectedDirTS * maxTraceDistance;
    
    float3 dPos = reflectionEndTS.xyz - samplePosTS.xyz;
    int2 sampleScreenPos = int2(samplePosTS.xy * ScreenSize);
    int2 endPosScreenPos = int2(reflectionEndTS.xy * ScreenSize);
    int2 dPos2 = endPosScreenPos - sampleScreenPos;
    int maxDist = max(abs(dPos2.x), abs(dPos2.y));
    dPos /= maxDist;
    dPos *= 2.0;
    
    int hitIndex = -1;
    int maxSteps = 500;
    float maxThickness = 0.005;
    int startDist = 1;
    
    float4 rayPosTS = float4(samplePosTS.xyz + dPos, 0.0);
    float4 rayDirTS = float4(dPos.xyz, 0.0);
    float4 rayStartPos = rayPosTS;
    rayPosTS += rayDirTS * startDist;
    
    for (int i = startDist; i < maxDist && i < maxSteps; i++)
    {
        float depth = depthMap.Sample(samp, rayPosTS.xy).r;
        float thickness = rayPosTS.z - depth;
        hitIndex = (thickness > 0.0 && thickness < maxThickness) ? i : hitIndex;
        if (hitIndex != -1)
            break;
        rayPosTS += rayDirTS;
    }
    bool intersected = hitIndex >= startDist;
    intersectionPosTS = rayStartPos.xyz + rayDirTS.xyz * hitIndex;
    return intersected;
}

float4 ComputeReflectedColor(bool intersected, float3 intersectionPosTS, float3 sceneColor, float metallic, float3 normal, float3 position)
{
    float2 dCoords = smoothstep(0.2, 0.6, abs(float2(0.5, 0.5) - intersectionPosTS.xy));
    float screenEdgeFactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);
    float multiplier = pow(metallic, 3.0) * screenEdgeFactor;
    
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, sceneColor, metallic);
    float3 fresnel = FresnelSchlick(max(dot(normalize(normal), normalize(position)), 0.0), F0);
    
    float4 ssrColor = intersected ? float4(pipelineOutputMap.Sample(samp, intersectionPosTS.xy).xyz, 1.0) : float4(0.0, 0.0, 0.0, 1.0);
    return ssrColor * clamp(multiplier, 0.0, 0.9) * float4(fresnel, 1.0);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 texCoord = input.texCoord;
    float3 sceneColor = pipelineOutputMap.Sample(samp, texCoord).xyz;
    float roughness = materialMap.Sample(samp, texCoord).r;
    float metallic = materialMap.Sample(samp, texCoord).g;
    float albedo = albedoMap.Sample(samp, texCoord).g;
    if (metallic < 0.01)
        return float4(sceneColor, 1.0);
    
    float3 normalWS = normalMap.Sample(samp, texCoord).xyz;
    float3 normalVS = mul(ViewMatrix, float4(normalWS, 0.0)).xyz;
    float3 positionWS = positionMap.Sample(samp, texCoord).xyz;
    
    float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 jitter = lerp(float3(0.0, 0.0, 0.0), float3(Hash(positionWS)), specularColor) * 0.2;
    
    float3 positionTS, reflectedDirTS, positionVS, reflectedDirVS, intersectionPosTS;
    float maxDistance;
    ComputePositionAndReflection(texCoord, normalVS, jitter, roughness, positionTS, reflectedDirTS, positionVS, maxDistance);
    bool intersected = FindIntersection(positionTS, reflectedDirTS, maxDistance, intersectionPosTS);
    float4 ssrColor = ComputeReflectedColor(intersected, intersectionPosTS, sceneColor, metallic, normalWS, positionVS);
    
    return float4(sceneColor + ssrColor.rgb, 1.0);
}