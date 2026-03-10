#include "../UniformsCommon.hlsl"

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 viewCenterPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    float thickness : SV_TARGET0;
};

[[vk::binding(1, 0)]] ConstantBuffer<FluidParams> fluidParams;

FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;

    float2 pointCoord = input.UV * 2.f - 1.f;
    float length2 = dot(pointCoord, pointCoord);
    if(length2 > 1.f) discard;

    float3 viewNormal = float3(pointCoord, sqrt(1.f - length2));
    float3 viewPos = input.viewCenterPos + viewNormal * fluidParams.pointSize;

    output.thickness = fluidParams.thicknessFactor * abs(viewPos.z);
    return output;
}