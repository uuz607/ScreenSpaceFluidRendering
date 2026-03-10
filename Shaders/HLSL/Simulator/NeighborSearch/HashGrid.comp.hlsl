#define THREAD_COUNT_X 256

struct HashGridParams
{
    float invSupportRadius;
    float supportRadius;
    float domainSizeXZ;
    uint cubeCountXZ;
    uint cubeCountY;
    uint cubeCount;
    uint particleCount;
    uint maxNeighborCount;
};
[[vk::binding(0, 0)]] ConstantBuffer<HashGridParams> params;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> particleCountsPerCube;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> cubeOffsets;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> particleIndicesInCube;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint> neighborCounts;
[[vk::binding(5, 0)]] RWStructuredBuffer<uint> neighborIndices;
[[vk::binding(6, 0)]] RWStructuredBuffer<float4> predictedPosition;


int3 getIndexInCube(uint index)
{   
    float3 shiftedPos = predictedPosition[index].xyz + float3(0.5f * params.domainSizeXZ, 0.0f, 0.5f * params.domainSizeXZ);
    return int3(floor(shiftedPos * params.invSupportRadius));
}

void getSurroundingIndexInCube(uint index, out int3 surroundingIndexInCube[27])
{
    int3 indexInCube = getIndexInCube(index);

    for (int i = 0; i < 3; ++i) 
        for (int j = 0; j < 3; ++j) 
            for (int k = 0; k < 3; ++k) 
            {
                surroundingIndexInCube[i * 9 + j * 3 + k] = int3(i - 1, j - 1, k - 1) + indexInCube;
            }
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void computeParticleCountsCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint particleIndex = GlobalInvocationID.x;
    if (particleIndex >= params.particleCount) return;

    int3 indexInCube = getIndexInCube(particleIndex);
    int3 stride = int3(params.cubeCountXZ * params.cubeCountY, params.cubeCountXZ, 1);
    int cubeIndex = dot(indexInCube, stride);

    InterlockedAdd(particleCountsPerCube[cubeIndex], 1);
}


[numthreads(1, 1, 1)]
void sumCubeOffsetsCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint cubeCount = params.cubeCount;
    
    cubeOffsets[0] = 0;
    for (int i = 1; i < cubeCount; i++) {
        cubeOffsets[i] = cubeOffsets[i - 1] + particleCountsPerCube[i - 1];
    }
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void assignParticlesCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint particleIndex = GlobalInvocationID.x;
    if (particleIndex >= params.particleCount) return;

    int3 indexInCube = getIndexInCube(particleIndex);
    int3 stride = int3(params.cubeCountXZ * params.cubeCountY, params.cubeCountXZ, 1);
    int cubeIndex = dot(indexInCube, stride);
    
    uint offsetIndex;
    InterlockedAdd(cubeOffsets[cubeIndex], 1, offsetIndex);
    particleIndicesInCube[offsetIndex] = particleIndex;
}


[numthreads(THREAD_COUNT_X, 1, 1)]
void performNeighborSearchCS(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
    uint particleIndex = GlobalInvocationID.x;
    if (particleIndex >= params.particleCount) return;

    int3 surroundingIndexInCube[27];
    getSurroundingIndexInCube(particleIndex, surroundingIndexInCube);

    uint neighborCount = 0;
    int3 maxIndexInCube = int3(params.cubeCountXZ, params.cubeCountY, params.cubeCountXZ);
    int3 stride = int3(params.cubeCountXZ * params.cubeCountY, params.cubeCountXZ, 1);

    for (int i = 0; i < 27; ++i)
    {
        if (all(surroundingIndexInCube[i] >= 0) && all(surroundingIndexInCube[i] < maxIndexInCube))
        {
            int cubeIndex = dot(surroundingIndexInCube[i], stride);

            for (uint j = 0; j < particleCountsPerCube[cubeIndex] ; ++j)
            {
                uint neighborIndex = particleIndicesInCube[cubeOffsets[cubeIndex] - 1 - j];
                
                if (neighborCount >= params.maxNeighborCount) break;

                if (particleIndex != neighborIndex)
                {
                    float3 rvec = (predictedPosition[particleIndex] - predictedPosition[neighborIndex]).xyz;
                    float r2 = dot(rvec, rvec);
                    float h2 = params.supportRadius * params.supportRadius;
                    if (r2 <= h2)
                    {
                        uint k = particleIndex * params.maxNeighborCount + neighborCount;
                        neighborIndices[k] = neighborIndex;
                        ++neighborCount;
                    }
                }
            }
        }
    }
    neighborCounts[particleIndex] = neighborCount;
}