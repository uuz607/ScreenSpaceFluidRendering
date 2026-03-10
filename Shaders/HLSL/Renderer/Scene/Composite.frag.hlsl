
struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    float4 Color : SV_TARGET0;
};

[[vk::binding(0, 0)]] Texture2D skyBoxColorTexture;
[[vk::binding(0, 0)]] SamplerState skyBoxColorSampler;

[[vk::binding(1, 0)]] Texture2D floorColorTexture;
[[vk::binding(1, 0)]] SamplerState floorColorSampler;

[[vk::binding(2, 0)]] Texture2D<int> objectIDTexture;

FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;
    int objectID = objectIDTexture.Load(int3(int2(input.Pos.xy), 0.f));
    
    if(objectID == 0)
    {
        output.Color = skyBoxColorTexture.Sample(skyBoxColorSampler, input.UV);
    }
    else if(objectID == 1)
    {
        output.Color = floorColorTexture.Sample(floorColorSampler, input.UV);
    }

    return output;
}