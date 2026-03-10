#include "../UniformsCommon.hlsl"

[[vk::binding(0, 0)]] ConstantBuffer<FluidParams> fluidParams;
[[vk::binding(1, 0)]] Texture2D<float4> inputNormalTexture;
[[vk::binding(2, 0)]] RWTexture2D<float4> outputNormalTexture;

bool isValidNormal(float3 normal)
{
    if(any(isnan(normal)) || dot(normal, normal) < 1e-6f) 
    {
        return false;
    }
    else
    {
        return true;
    }
        
}

static const int radius = 1;

[numthreads(THREAD_COUNT_2D_X, THREAD_COUNT_2D_Y, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 texelCoord = int2(GlobalInvocationID.xy);
    if (any(texelCoord >= fluidParams.screenSize)) return;

    float3 normal = inputNormalTexture.Load(int3(texelCoord, 0)).xyz;
    if(isValidNormal(normal))
    {
        outputNormalTexture[texelCoord] = float4(normal, 0.f);
        return;
    }

    float3 sum = 0.f;
    int count = 0;

    for(int dy = -radius; dy <= radius; ++dy)
    {
        for(int dx = -radius; dx <= radius; ++dx)
        {
            int2 neighborCoord = texelCoord + int2(dx, dy);
            if(any(neighborCoord < 0) || any(neighborCoord >= fluidParams.screenSize)) continue;

            float3 neighborNormal = inputNormalTexture.Load(int3(neighborCoord, 0)).xyz;
            if(isValidNormal(neighborNormal))
            {
                sum += neighborNormal;
                ++count;
            }
        }
    }
    
    if(count > 0)
    {
        outputNormalTexture[texelCoord] = float4(normalize(sum / count), 0.f);
    }
    else
    {
        outputNormalTexture[texelCoord] = float4(0.f, 0.f, 1.f, 0.f);
    }  
}