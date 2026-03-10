struct VSOutput
{
    [[vk::location(0)]] float3 UV : TEXCOORD0;
};

struct FSOutput
{
    float4 Color : SV_TARGET0;
    int ObjectID : SV_TARGET1;
};

[[vk::binding(1, 0)]] TextureCube  skyboxTexture;
[[vk::binding(1, 0)]] SamplerState skyboxSampler;

FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;
    output.Color = skyboxTexture.Sample(skyboxSampler, input.UV);
    output.ObjectID = 0;
    return output;
}