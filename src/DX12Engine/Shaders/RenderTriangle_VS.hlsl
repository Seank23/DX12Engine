struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    float2 positions[3] = 
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };
    float2 texCoords[3] =
    {
        float2(0.0, 0.0),
        float2(0.0, 2.0),
        float2(2.0, 0.0)
    };
    VSOutput output;
    output.position = float4(positions[vertexID], 1.0, 1.0);
    output.texCoord = texCoords[vertexID];
    output.texCoord.y = 1.0 - output.texCoord.y; // Flip Y coordinate
    return output;
}