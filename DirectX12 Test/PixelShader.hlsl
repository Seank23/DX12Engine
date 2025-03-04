struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct Light
{
    float3 Position;
    float Intensity;
    float3 Direction;
    float Range;
    float3 Color;
    float SpotAngle;
};

cbuffer LightBuffer : register(b0)
{
    int LightCount;
    float3 Padding;
    Light Lights[4];
};

cbuffer MaterialData : register(b2)
{
    float4 BaseColor;
    int HasTexture;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float3 ComputeLighting(float3 normal, float3 worldPos)
{
    float3 finalColor = float3(0, 0, 0);

    for (int i = 0; i < LightCount; i++)
    {
        Light light = Lights[i];
        
        float3 lightDir = normalize(light.Position - worldPos);
        float diff = max(dot(normal, lightDir), 0.0);
        
        finalColor += light.Color * diff * light.Intensity;
    }

    return finalColor;
}

float4 main(PSInput input) : SV_TARGET
{
    float4 color = BaseColor;
    if (HasTexture == 1)
    {
        color *= gTexture.Sample(gSampler, input.texCoord);
    }
    float3 normal = normalize(input.normal);
    float3 lighting = ComputeLighting(normal, input.worldPos);
    
    return float4(lighting * color.xyz, 1.0f);
}
