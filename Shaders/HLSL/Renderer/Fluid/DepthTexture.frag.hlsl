#include "../UniformsCommon.hlsl"

struct VSOutput
{
    [[vk::location(0)]] float3 viewCenterPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    int Valid : SV_TARGET0;
    float SmoothedDepth : SV_TARGET1;
    float Depth : SV_DEPTH;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;
[[vk::binding(1, 0)]] ConstantBuffer<FluidParams> fluidParams;


FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;

    float2 pointCoord = input.UV * 2.f - 1.f;
    float length2 = dot(pointCoord, pointCoord);
    if(length2 > 1.f) discard;

    float3 viewNormal = float3(pointCoord, sqrt(1.f - length2));
    float3 viewPos = input.viewCenterPos + viewNormal * fluidParams.pointSize;
    float4 clipPos = mul(cameraParams.proj, float4(viewPos, 1.f));

    output.Depth = clipPos.z / clipPos.w;
    output.SmoothedDepth = output.Depth;
    output.Valid = 1;
    return output;
}