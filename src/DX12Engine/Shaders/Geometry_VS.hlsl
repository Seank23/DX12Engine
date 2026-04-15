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
    float4 currentPosition : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 prevClip : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 normal : TEXCOORD3;
    float2 uv : TEXCOORD4;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(ModelMatrix, float4(input.position, 1.0f));
    float4 currentClip = mul(MVPMatrix, float4(input.position, 1.0f));
    output.currentPosition = currentClip;
    output.currentClip = currentClip;
    output.prevClip = mul(PrevMVPMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPosition.xyz;
    output.normal   = normalize(mul(NormalMatrix, float4(input.normal, 0.0f)).xyz);
    output.uv       = input.texCoord;
    float3 t        = normalize(mul((float3x3) ModelMatrix, input.tangent.xyz));
    output.tangent   = t;
    // Reconstruct bitangent with handedness sign so mirrored UVs flip correctly.
    output.bitangent = cross(output.normal, t) * input.tangent.w;
    return output;
}
