struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    float4 worldPosition = float4(input.position, 1.0);
    output.position = mul(worldPosition, modelViewProjectionMatrix);
    return output;
}