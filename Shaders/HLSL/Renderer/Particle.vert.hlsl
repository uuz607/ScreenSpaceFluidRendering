#include "UniformsCommon.hlsl"

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 viewCenterPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;
[[vk::binding(1, 0)]] ConstantBuffer<FluidParams> fluidParams;

[[vk::binding(2, 0)]] StructuredBuffer<float4> positions;


static const float2 offsets[6] = {
    float2(-1, -1), 
    float2(-1,  1), 
    float2( 1, -1), 
    float2( 1, -1), 
    float2(-1,  1), 
    float2( 1,  1) 
};

static const float2 texCoords[6] = {
    float2(0, 0),
    float2(0, 1),
    float2(1, 0),
    float2(1, 0),
    float2(0, 1),
    float2(1, 1)
};


VSOutput main(uint VertexIndex : SV_VertexID, uint InstanceIndex : SV_InstanceID)
{
    VSOutput output = (VSOutput)0;

    float4 worldPos = float4(positions[InstanceIndex].xyz, 1.f);
    float4 viewPos = mul(cameraParams.view, mul(fluidParams.model, worldPos));

    uint idx = VertexIndex;

    float2 offset = offsets[idx] * fluidParams.pointSize;
    float4 viewQuadPos = viewPos + float4(offset, 0.f, 0.f);

    output.Pos = mul(cameraParams.proj, viewQuadPos);
    output.viewCenterPos = viewPos.xyz;
    output.UV = texCoords[idx];
    return output;
}