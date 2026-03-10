#include "../UniformsCommon.hlsl"

[[vk::binding(0, 0)]] ConstantBuffer<FluidParams> fluidParams;
[[vk::binding(1, 0)]] Texture2D<float> inputDepthTexture;
[[vk::binding(2, 0)]] RWTexture2D<float> outputDepthTexture;
[[vk::binding(3, 0)]] Texture2D<int> validTexture;

struct PushConsts
{
    int axis;
    int kernelRadius;
};
[[vk::push_constant]] PushConsts pushConsts;

static const float sigma_d = 7.f;
static const float sigma_r = 0.02f;
static const float inv_r2 =  1.f / (2.f * sigma_r * sigma_r);
static const float inv_d2 =  1.f / (2.f * sigma_d * sigma_d);

bool isValidNeighbor(int2 texelCoord)
{
    return all(texelCoord >= 0) && 
           all(texelCoord < fluidParams.screenSize) &&
           validTexture.Load(int3(texelCoord, 0.f)) != 0;
}


[numthreads(THREAD_COUNT_2D_X, THREAD_COUNT_2D_Y, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    int2 texelCoord = int2(GlobalInvocationID.xy);
    if (any(texelCoord >= fluidParams.screenSize)) return;

    int valid = validTexture.Load(int3(texelCoord, 0));
    if(valid == 0) return;

    // two-step bilateral blur
    float depth = inputDepthTexture.Load(int3(texelCoord, 0));

    int radius = pushConsts.kernelRadius;
    int axis = pushConsts.axis;

    float w = 0.f;
    float sum = 0.f;

    for(int i = -radius; i <= radius; ++i)
    {
        int2 neighborCoord;
        if(axis == 0) // horizontal 
        {
            neighborCoord = texelCoord + int2(i, 0);
        }
        else if (axis == 1) // vertical
        {
            neighborCoord = texelCoord + int2(0, i);
        }
        else
        {
            break;
        }
            
        if(!isValidNeighbor(neighborCoord)) continue;

        float2 dxy = float2(i, 0.f);
        float  f_d = exp(-dot(dxy, dxy) * inv_d2);

        float neighborDepth = inputDepthTexture.Load(int3(neighborCoord, 0.f));
        float d_depth = neighborDepth - depth;
        float f_r = exp(-d_depth * d_depth * inv_r2);

        w += f_r * f_d;
        sum += neighborDepth * f_r * f_d; 
    }

    if(w == 0.f)
        outputDepthTexture[texelCoord] = depth;
    else
        outputDepthTexture[texelCoord] = sum / w;
}