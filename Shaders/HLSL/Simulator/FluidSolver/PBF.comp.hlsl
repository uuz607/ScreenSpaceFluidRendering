#define THREAD_COUNT_X 256
#define PI 3.14159265358979323846

struct PBFParams
{
    float4 gravity;

    float supportRadius;
    float volume0;
    float invRho0;
    float lambdaEps;

    float domainSizeXZ;
    float domainSizeY;
    float dt;
    float invDt;

    float xsphCoeff;
    float vorticityCoeff;
    uint particleCount;
    uint maxNeighborCount;
};
[[vk::binding(0, 0)]] ConstantBuffer<PBFParams> params;

[[vk::binding(1, 0)]] RWStructuredBuffer<float4> position;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> predictedPosition;
[[vk::binding(3, 0)]] RWStructuredBuffer<float4> velocity;
[[vk::binding(4, 0)]] RWStructuredBuffer<float4> vorticity;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4> deltaPosition;
[[vk::binding(6, 0)]] RWStructuredBuffer<float> lambda;

[[vk::binding(7, 0)]] RWStructuredBuffer<uint> neighborCounts;
[[vk::binding(8, 0)]] RWStructuredBuffer<uint> neighborIndices;

[[vk::binding(9, 0)]] Texture1D<float2> sphKernel;
[[vk::binding(9, 0)]] SamplerState sphKernelSampler;



[numthreads(THREAD_COUNT_X, 1, 1)]
void predictPositionCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;

    velocity[i] += params.gravity * params.dt;
    predictedPosition[i] = position[i] + velocity[i] * params.dt;
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void computeLambdaCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;
    
    float h = params.supportRadius;
    float volume0 = params.volume0;
    float invRho0 = params.invRho0;

    float density = sphKernel.SampleLevel(sphKernelSampler, 0.f, 0).x;
    for (uint k = 0; k < neighborCounts[i]; ++k)
    {
        uint j = neighborIndices[i * params.maxNeighborCount + k];
        float3 xij = (predictedPosition[i] - predictedPosition[j]).xyz;
        float r = length(xij);
        density += sphKernel.SampleLevel(sphKernelSampler, r / h, 0).x ; //poly6(xij);
    }

    lambda[i] = 0.f;
    float C = max(density * volume0 - 1.f, 0.f); // rho*mass0/rho0 = rho*volume0
    if (C != 0.f)
    {
        float3 gradCi = 0.f;
        float gradC2 = 0.f;
        for (uint k = 0; k < neighborCounts[i]; ++k)
        {
            uint j = neighborIndices[i * params.maxNeighborCount + k];
            float3 xij = (predictedPosition[i] - predictedPosition[j]).xyz;
            float r = length(xij);
            float3 gradCj = volume0 * sphKernel.SampleLevel(sphKernelSampler, r / h, 0).y * xij;
            gradC2 += dot(gradCj, gradCj);
            gradCi += gradCj;
        }
        gradC2 += dot(gradCi, gradCi);
        lambda[i] = -C / (gradC2 + params.lambdaEps);
    }
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void computeDeltaPositionCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;
    
    float h = params.supportRadius;
    float volume0 = params.volume0;
    float3 deltaPos = 0.f;
    for (uint k = 0; k < neighborCounts[i]; ++k)
    {
        uint j = neighborIndices[i * params.maxNeighborCount + k];
        float3 xij = (predictedPosition[i] - predictedPosition[j]).xyz;
        float r = length(xij);
        float3 gradCj = sphKernel.SampleLevel(sphKernelSampler, r / h, 0).y * xij;

        deltaPos += (lambda[i] + lambda[j]) * gradCj;
    }
    deltaPosition[i].xyz = volume0 * deltaPos;
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void handleBoundaryCollisionCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;

    float eps = 0.1f;
    float domainSizeXZ = params.domainSizeXZ;
    float domainSizeY = params.domainSizeY;

    float3 pos = (predictedPosition[i] + deltaPosition[i]).xyz;
    float3 minDomain = float3(-0.5f * domainSizeXZ + eps, eps, -0.5f * domainSizeXZ + eps);
    float3 maxDomain = float3(0.5f * domainSizeXZ - eps, domainSizeY - eps, 0.5f * domainSizeXZ - eps);
    pos = clamp(pos, minDomain, maxDomain);

    deltaPosition[i].xyz = pos - predictedPosition[i].xyz;
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void adjustPredictedPositionCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;

    predictedPosition[i] += deltaPosition[i];
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void updateParticleCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;

    velocity[i] = (predictedPosition[i] - position[i]) * params.invDt;
    position[i] = predictedPosition[i];
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void xsphCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint i = GlobalInvocationID.x;
    if (i >= params.particleCount) return;

    float h = params.supportRadius;
    float3 xpsh = 0.f;
    for (uint k = 0; k < neighborCounts[i]; ++k)
    {
        uint j = neighborIndices[i * params.maxNeighborCount + k];
        float3 xij = (predictedPosition[i] - predictedPosition[j]).xyz;
        float3 vji = (velocity[j] - velocity[i]).xyz;
        float r = length(xij);
        float W = sphKernel.SampleLevel(sphKernelSampler, r / h, 0).x;
        xpsh += vji * W;
    }

    velocity[i].xyz += params.invRho0 * params.xsphCoeff * xpsh;
}