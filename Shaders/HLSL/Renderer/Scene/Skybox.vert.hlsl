#include "../UniformsCommon.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;
};

struct VSOutput
{   
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 UV : TEXCOORD0;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.UV = input.Pos;
    // Convert cubemap coordinates into Vulkan coordinate space
	output.UV.xy *= -1.f;
    output.Pos = mul(cameraParams.projView, float4(input.Pos, 1.f)).xyww;
    return output;
}