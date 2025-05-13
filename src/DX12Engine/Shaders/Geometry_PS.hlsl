Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D aoMap : register(t4);
SamplerState samp : register(s0);

cbuffer MaterialData : register(b1)
{
    float3 Albedo;
    float Metallic;
    float Roughness;
    float AO;
    float3 Emissive;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 objectNormal : SV_Target2;
    float4 material : SV_Target3; // roughness, metallic, etc.
    float4 position : SV_Target4;
};

PSOutput main(PSInput input)
{
    PSOutput output;

    float3 baseColor = albedoMap.Sample(samp, input.uv);
    float metallic = metallicMap.Sample(samp, input.uv);
    float roughness = roughnessMap.Sample(samp, input.uv);
    float ao = aoMap.Sample(samp, input.uv);
    float3 textureNormal = normalMap.Sample(samp, input.uv).rgb * 2.0 - 1.0;
    float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
    float3 worldNormal = normalize(mul(textureNormal, TBN));

    output.albedo = float4(baseColor, 1.0);
    output.worldNormal = float4(worldNormal, 1.0);
    output.objectNormal = float4(input.normal, 1.0);
    output.material = float4(roughness, metallic, ao, 1);
    output.position = float4(input.worldPos, 1.0);
    return output;
}