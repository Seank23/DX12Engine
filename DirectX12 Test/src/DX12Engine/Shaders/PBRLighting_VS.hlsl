cbuffer ConstantBuffer : register(b1)
{
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 MVPMatrix;
    float3 CameraPosition;
};

struct Light
{
    int Type; // 0 = Directional, 1 = Point, 2 = Spot
    float3 Position;
    float Intensity;
    float3 Direction;
    float Range;
    float3 Color;
    float SpotAngle;
    float3 Padding;
    matrix ViewProjMatrix;
};

cbuffer LightBuffer : register(b0)
{
    int LightCount;
    float3 Padding;
    Light Lights[4];
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 cameraPos : POSITION0;
    float3 worldPos : POSITION1;
    float4 lightSpacePos : POSITION2;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.cameraPos = CameraPosition;
    float4 worldPos = mul(ModelMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;
    output.position = mul(MVPMatrix, float4(input.position, 1.0f));
    output.lightSpacePos = mul(Lights[0].ViewProjMatrix, worldPos);
    output.normal = input.normal;
    float4 tangent = normalize(mul(ModelMatrix, float4(input.tangent, 1.0)));
    output.tangent = tangent.xyz / tangent.w;
    output.bitangent = cross(output.tangent, output.normal);
    output.texCoord = input.texCoord;
    return output;
}
