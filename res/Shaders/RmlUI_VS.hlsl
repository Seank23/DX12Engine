cbuffer UiConstants : register(b0)
{
    float4x4 Transform;
    float2 Translation;
    float2 Padding;
};

struct VSInput
{
    float2 position : POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float2 translated = input.position + Translation;
    output.position = mul(Transform, float4(translated, 0.0f, 1.0f));
    output.color = input.color;
    output.uv = input.uv;
    return output;
}
