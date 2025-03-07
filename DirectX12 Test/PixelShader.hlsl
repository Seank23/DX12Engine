struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
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
        float3 lightDir = 0;
        float attenuation = 1.0;
        
        if (light.Type == 0)
        {
            lightDir = normalize(-light.Direction);
        }
        else if (light.Type == 1)
        {
            float3 lightVector = light.Position - worldPos;
            lightDir = normalize(lightVector);
            float dist = length(lightVector);
            attenuation = saturate(1.0 - (dist / light.Range));
        }
        else if (light.Type == 2)
        {
            float3 lightVector = light.Position - worldPos;
            lightDir = normalize(lightVector);
            float dist = length(lightVector);
            float spotFactor = dot(normalize(light.Direction), -lightDir);
            float spotAttenuation = smoothstep(light.SpotAngle, light.SpotAngle + 0.1, spotFactor);
            attenuation = spotAttenuation * saturate(1.0 - (dist / light.Range));
        }
        float NdotL = max(dot(normal, lightDir), 0.0);  
        finalColor += light.Color * light.Intensity * NdotL * attenuation;
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
    float3 lighting = ComputeLighting(normalize(input.normal), input.worldPos);
    
    float3 finalColor = lighting * color.xyz;
    return float4(finalColor, 1.0f);
}
