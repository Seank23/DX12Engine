struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    return float4(input.texCoord, 0.5f, 1.0f); // Output the interpolated vertex color
    //return input.color;
}
