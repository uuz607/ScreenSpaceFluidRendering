#include "../UniformsCommon.hlsl"

[[vk::binding(0, 0)]] ConstantBuffer<CameraParams> cameraParams;
[[vk::binding(1, 0)]] ConstantBuffer<FluidParams> fluidParams;
[[vk::binding(2, 0)]] Texture2D<float> depthTexture;
[[vk::binding(3, 0)]] RWTexture2D<float4> viewNormalTexture;

float3 getViewPos(int2 texelCoord)
{   
    float2 uv = float2(texelCoord) / float2(fluidParams.screenSize);
    float depth = depthTexture.Load(int3(texelCoord, 0));
    float3 ndcPos = float3(2 * uv.x - 1.f, 1.f - 2 * uv.y, depth);

    float4 pos = mul(cameraParams.invProj, float4(ndcPos, 1.f));
    return pos.xyz / pos.w;
}

[numthreads(THREAD_COUNT_2D_X, THREAD_COUNT_2D_Y, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 texelCoord = int2(GlobalInvocationID.xy);
    if(any(texelCoord >= fluidParams.screenSize)) return;

    float3 viewPos = getViewPos(texelCoord);
    float3 neighborXPos = getViewPos(texelCoord + int2( 1,  0));
    float3 neighborXNeg = getViewPos(texelCoord + int2(-1,  0));
    float3 neighborYPos = getViewPos(texelCoord + int2( 0,  1)); 
    float3 neighborYNeg = getViewPos(texelCoord + int2( 0, -1));

    float3 tangentX = abs(neighborXPos.z - viewPos.z) < abs(neighborXNeg.z - viewPos.z)? neighborXPos - viewPos : viewPos - neighborXNeg;
    float3 tangentY = abs(neighborYPos.z - viewPos.z) < abs(neighborYNeg.z - viewPos.z)? neighborYPos - viewPos : viewPos - neighborYNeg;

    float3 normal = normalize(cross(tangentX, tangentY));

    viewNormalTexture[texelCoord] = float4(normal, 0.f);
}
