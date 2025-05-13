struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    float3 positions[3] = 
    {
        float3(-1.0, -1.0, 1.0),
        float3(-1.0, 3.0, 1.0),
        float3(3.0, -1.0, 1.0)
    };
    VSOutput output;
    output.position = float4(positions[vertexID], 1.0);
    output.texCoord = output.position.xy * 0.5 + 0.5;
    output.texCoord.y = 1.0 - output.texCoord.y; // Flip Y coordinate
    return output;
}