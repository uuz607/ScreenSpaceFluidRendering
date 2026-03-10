#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "EnumArray.hpp"

#include <cstdint>
#include <glm/glm.hpp>

namespace vks {
    class VulkanContext;
    class CommandList;  
}

namespace ssfr
{
    struct HashGridResources
    {
        vks::DeviceBuffer<glm::vec4>* predictedPosition = nullptr;
    };

    class HashGrid
    {
    public:
        HashGrid() = default;
        ~HashGrid() = default;

        vks::Buffer& neighborCountBuffer() { return storageBuffers_.neighborCount; }
        vks::Buffer& neighborIndexBuffer() { return storageBuffers_.neighborIndex; }
        
        VkDescriptorBufferInfo getNeighborCountDescriptor() const { return storageBuffers_.neighborCount.descriptorInfo(); }
        VkDescriptorBufferInfo getNeighborIndexDescriptor() const { return storageBuffers_.neighborIndex.descriptorInfo(); }
        
        void create(vks::VulkanContext* context);
        void setResources(const HashGridResources& resources);
        void execute(vks::CommandList& commandList);

    private:
        void prepareUniformBuffer();
        void createStorageBuffers();
        void setupDescriptors();
        void preparePipelines();

    private:
        struct StorageBuffers 
        {
            vks::DeviceBuffer<uint32_t> particleCountPerCube;
            vks::DeviceBuffer<uint32_t> cubeOffset;
            vks::DeviceBuffer<uint32_t> particleIndexInCube;
            vks::DeviceBuffer<uint32_t> neighborCount;
            vks::DeviceBuffer<uint32_t> neighborIndex;
        };

        enum class PipelineID : uint8_t
        {
            ComputeParticleCounts,
            SumCubeOffsets,
            AssignParticles,
            PerformNeighborSearch,
            Count
        };

        struct Params
        {
            float invSupportRadius;
            float supportRadius;
            float domainSizeXZ;
            uint32_t cubeCountXZ;
            uint32_t cubeCountY;
            uint32_t cubeCount;
            uint32_t particleCount;
            uint32_t maxNeighborCount;
        };

        using Pipelines = utils::EnumArray<vks::Pipeline, PipelineID>;

    private:
        vks::VulkanContext* context_ = nullptr;

        HashGridResources resources_;

        vks::UniformBuffer<Params> uniformBuffer_;
        StorageBuffers storageBuffers_;

        vks::DescriptorSet descriptorSet_;
        Pipelines pipelines_;
    };
}