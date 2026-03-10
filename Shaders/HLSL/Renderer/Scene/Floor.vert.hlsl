#include "../UniformsCommon.hlsl"

struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.Pos = mul(cameraParams.projView, float4(input.Pos, 1.f));
    output.WorldPos = input.Pos;
    output.UV = input.UV;
    return output;
}