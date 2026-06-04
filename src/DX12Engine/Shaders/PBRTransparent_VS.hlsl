cbuffer ObjectData : register(b0)
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 MVPMatrix;
    float3 CameraPosition;
    float Padding;
    float4x4 PrevMVPMatrix;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
    float4 tangent  : TANGENT; // xyz = tangent, w = handedness sign
};

struct VSOutput
{
    float4 positionCS  : SV_POSITION;
    float3 worldPos    : TEXCOORD0;
    float3 normalWS    : TEXCOORD1;
    float2 uv          : TEXCOORD2;
    float3 tangentWS   : TEXCOORD3;
    float3 bitangentWS : TEXCOORD4;
    float3 viewDirWS   : TEXCOORD5;
};

VSOutput main(VSInput input)
{
    VSOutput o;

    float4 worldPos = mul(ModelMatrix, float4(input.position, 1.0));
    o.positionCS = mul(MVPMatrix, float4(input.position, 1.0));
    o.worldPos   = worldPos.xyz;

    o.normalWS  = normalize(mul(NormalMatrix, float4(input.normal, 0.0)).xyz);

    float3 t    = normalize(mul((float3x3) ModelMatrix, input.tangent.xyz));
    o.tangentWS = t;
    // Reconstruct bitangent with handedness sign so mirrored UVs flip correctly.
    o.bitangentWS = cross(o.normalWS, t) * input.tangent.w;

    o.uv         = input.texCoord;
    o.viewDirWS  = normalize(CameraPosition - o.worldPos);
    return o;
}