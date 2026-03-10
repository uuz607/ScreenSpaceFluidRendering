#include "../UniformsCommon.hlsl"

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 viewCenterPos : POSITION0;
    [[vk::location(1)]] float2 UV : TEXCOORD0;
};

struct FSOutput
{
    float4 Color : SV_TARGET0;
    float Depth : SV_DEPTH;
};


[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;
[[vk::binding(1, 0)]] ConstantBuffer<FluidParams> fluidParams;

[[vk::binding(3, 0)]] Texture2D viewNormalTexture;
[[vk::binding(3, 0)]] SamplerState viewNormalSampler;

[[vk::binding(4, 0)]] Texture2D<float> thicknessTexture;
[[vk::binding(4, 0)]] SamplerState thicknessSampler;

[[vk::binding(5, 0)]] Texture2D<float> smoothedDepthTexture;
[[vk::binding(5, 0)]] SamplerState smoothedDepthSampler;

[[vk::binding(6, 0)]] Texture2D sceneColorTexture;
[[vk::binding(6, 0)]] SamplerState sceneColorSampler;

[[vk::binding(7, 0)]] TextureCube skyboxTexture;
[[vk::binding(7, 0)]] SamplerState skyboxSampler;


FSOutput main(VSOutput input)
{
    FSOutput output = (FSOutput)0;

    float2 pointCoord = input.UV * 2.f - 1.f;
    float length2 = dot(pointCoord, pointCoord);
    if(length2 > 1.f) discard;

    float2 screenUV = input.Pos.xy / fluidParams.screenSize;

    float3 viewNormal = viewNormalTexture.Sample(viewNormalSampler, screenUV).xyz;
    float3 worldNormal = normalize(mul(cameraParams.invView, float4(viewNormal, 0.f)).xyz);
    float3 viewPos = input.viewCenterPos + viewNormal * fluidParams.pointSize;
    float3 worldPos = mul(cameraParams.invView, float4(viewPos, 1.f)).xyz;

    float3 I = normalize(cameraParams.position.xyz - worldPos);
    float3 N = normalize(worldNormal);

    // refract
    float thickness = thicknessTexture.Sample(thicknessSampler, screenUV);
    float3 attenuationFactor = exp(-(1.f - fluidParams.color.xyz) * thickness);
    float eta = 1.f / 1.33f;
    float refractScaler = refract(I, N, eta).y * 0.025f;
    float2 refractUV = screenUV + viewNormal.xy * refractScaler;
    float3 refractColor = sceneColorTexture.Sample(sceneColorSampler, refractUV).rgb * attenuationFactor;

    // reflect
    float3 reflectDir = normalize(reflect(-I, N));
    float3 reflectColor = skyboxTexture.Sample(skyboxSampler, reflectDir).rgb;

    output.Color = float4(refractColor + 0.75f * reflectColor, 1.f);
    output.Depth = smoothedDepthTexture.Sample(smoothedDepthSampler, screenUV);
    return output;
}