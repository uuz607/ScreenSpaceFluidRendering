
struct VSOutput
{
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    float4 Position : SV_TARGET0;
    float4 Color : SV_TARGET1;
    int ObjectID : SV_TARGET2;
};

[[vk::binding(1, 0)]] Texture2D floorTexture;
[[vk::binding(1, 0)]] SamplerState floorSampler;

FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;
    output.Position = float4(input.WorldPos, 1.f);
    output.Color = floorTexture.Sample(floorSampler, input.UV);
    output.ObjectID = 1;
    return output;
}