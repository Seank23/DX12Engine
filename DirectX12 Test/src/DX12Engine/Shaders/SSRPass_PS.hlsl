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

static float rayStep = 0.1;
static float minRayStep = 0.1;
static int maxSteps = 30;
static float searchDist = 5;
static int numSearchSteps = 5;

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

float3 BinarySearch(in float3 direction, inout float3 hitCoord, out float dDepth)
{
    float depth = 0.0;
    float4 projectedCoord = float4(0, 0, 0, 0);
    
    for(int i = 0; i < numSearchSteps; i++)
    {
        projectedCoord = mul(ProjectionMatrix, float4(hitCoord, 1.0));
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
        
        depth = depthMap.Sample(samp, projectedCoord.xy).r;
        dDepth = hitCoord.z - depth;
        
        direction *= 0.5;
        if (dDepth > 0.0)
            hitCoord += direction;
        else
            hitCoord -= direction;
    }
    projectedCoord = mul(ProjectionMatrix, float4(hitCoord, 1.0));
    projectedCoord.xy /= projectedCoord.w;
    projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
    return float3(projectedCoord.xy, depth);
}

float4 RayCast(in float3 direction, inout float3 hitCoord, out float dDepth)
{
    direction *= rayStep;
    float depth = 0.0;
    float4 projectedCoord = float4(0, 0, 0, 0);
    
    for (int i = 0; i < maxSteps; i++)
    {
        hitCoord += direction;
        
        projectedCoord = mul(ProjectionMatrix, float4(hitCoord, 1.0));
        projectedCoord.xy /= projectedCoord.w;
        projectedCoord.xy = projectedCoord.xy * 0.5 + 0.5;
        
        depth = depthMap.Sample(samp, projectedCoord.xy).r;
        
        if (depth > 1000.0)
            continue;
        
        dDepth = hitCoord.z - depth;
        
        if (direction.z - dDepth < 1.2)
        {
            if (dDepth <= 0.0)
            {
                float4 result;
                result = float4(BinarySearch(direction, hitCoord, dDepth), 1.0);
            }
        }
    }
    return float4(projectedCoord.xy, depth, 1.0);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 texCoord = input.texCoord;
    float3 sceneColor = pipelineOutputMap.Sample(samp, texCoord).xyz;
    float metallic = materialMap.Sample(samp, texCoord).g;
    float albedo = albedoMap.Sample(samp, texCoord).g;
    if (metallic < 0.01)
        return float4(sceneColor, 1.0);
    
    float3 normalWS = normalMap.Sample(samp, texCoord).xyz;
    float3 normalVS = mul(ViewMatrix, float4(normalWS, 0.0)).xyz;
    float3 positionWS = positionMap.Sample(samp, texCoord).xyz;
    float3 positionVS = mul(ViewMatrix, float4(positionWS, 1.0)).xyz;
    
    float3 reflectedDir = normalize(reflect(-normalize(positionVS), normalize(normalVS)));
    float3 hitPosition = positionVS;
    float dDepth = 0.0;
    
    float3 specularColor = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 jitter = lerp(float3(0.0, 0.0, 0.0), float3(Hash(positionWS)), specularColor) * 0.05;
    
    float4 coords = RayCast(jitter + reflectedDir * max(minRayStep, -positionVS.z), hitPosition, dDepth);
    float2 dCoords = smoothstep(0.2, 0.6, abs(float2(0.5, 0.5) - coords.xy));
    float screenEdgeFactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);
    float multiplier = pow(metallic, 3.0) * screenEdgeFactor * -reflectedDir.z;
    
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 fresnel = FresnelSchlick(max(dot(normalize(normalVS), normalize(positionVS)), 0.0), F0);
    
    float3 SSR = pipelineOutputMap.Sample(samp, coords.xy).xyz * clamp(multiplier, 0.0, 0.9) * fresnel;
    
    return float4(sceneColor + SSR, 1.0);
}