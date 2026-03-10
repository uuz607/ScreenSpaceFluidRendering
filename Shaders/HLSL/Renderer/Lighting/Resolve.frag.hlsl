
struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    float4 Color : SV_TARGET0;
};

[[vk::binding(0, 0)]] Texture2D sceneColorTexture;
[[vk::binding(0, 0)]] SamplerState sceneColorSampler;

[[vk::binding(1, 0)]] Texture2D<float> sceneDepthTexture;
[[vk::binding(1, 0)]] SamplerState sceneDepthSampler;

[[vk::binding(2, 0)]] Texture2D fluidColorTexture;
[[vk::binding(2, 0)]] SamplerState fluidColorSampler;

[[vk::binding(3, 0)]] Texture2D<float> fluidDepthTexture;
[[vk::binding(3, 0)]] SamplerState fluidDepthSampler;

[[vk::binding(4, 0)]] Texture2D<int> fluidValidTexture;

FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;

    output.Color = sceneColorTexture.Sample(sceneColorSampler, input.UV);
    float sceneDepth = sceneDepthTexture.Sample(sceneDepthSampler, input.UV);

    int fluidValid = fluidValidTexture.Load(int3(int2(input.Pos.xy), 0));
    if(fluidValid == 1)
    {
        float fluidDepth = fluidDepthTexture.Sample(fluidDepthSampler, input.UV);
        if(fluidDepth <= sceneDepth)
        {
            output.Color = fluidColorTexture.Sample(fluidColorSampler, input.UV);
        }
    }
    
    return output;
}