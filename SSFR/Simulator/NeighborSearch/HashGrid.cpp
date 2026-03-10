#include "HashGrid.h"
#include "VulkanContext.h"
#include "VulkanShader.hpp"
#include "AssetLoader.h"
#include "VulkanCommand.h"
#include "VulkanTools.h"

#include "Simulator/Parameters.h"

namespace ssfr
{
    static const uint32_t CubeCountY = static_cast<uint32_t>(ceil(DomainSizeY / SupportRadius));
    static const uint32_t CubeCountXZ = static_cast<uint32_t>(ceil(DomainSizeXZ / SupportRadius));
    static const uint32_t CubeCount = CubeCountXZ * CubeCountXZ * CubeCountY;

    void HashGrid::create(vks::VulkanContext* context)
    {
        context_ = context;
        prepareUniformBuffer();
        createStorageBuffers();
        setupDescriptors();
        preparePipelines();
    }

    void HashGrid::setResources(const HashGridResources& resources)
    {
        resources_ = resources;

        // Update descriptor set
        VkDescriptorBufferInfo uniformDescInfo = uniformBuffer_.descriptorInfo();
        VkDescriptorBufferInfo particleCountDescInfo = storageBuffers_.particleCountPerCube.descriptorInfo();
        VkDescriptorBufferInfo cubeOffsetDescInfo = storageBuffers_.cubeOffset.descriptorInfo();
        VkDescriptorBufferInfo particleIndexInCubeDescInfo = storageBuffers_.particleIndexInCube.descriptorInfo();
        VkDescriptorBufferInfo neighborCountDescInfo = storageBuffers_.neighborCount.descriptorInfo();
        VkDescriptorBufferInfo neighborIndexDescInfo = storageBuffers_.neighborIndex.descriptorInfo();
        VkDescriptorBufferInfo positionDescInfo = resources_.predictedPosition->descriptorInfo();

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &particleCountDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &cubeOffsetDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &particleIndexInCubeDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &neighborCountDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &neighborIndexDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &positionDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void HashGrid::execute(vks::CommandList& commandList)
    {
        // Record
        vks::CommandContext commandContext;

        // Clear particle counts
        {
            commandContext.useBuffer(storageBuffers_.particleCountPerCube, vks::BufferUsage::TransferDst);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.fillBuffer(storageBuffers_.particleCountPerCube, 0);
        }

        // Compute particle counts
        {
            uint32_t groupCountX = (ParticleCount + ThreadGroupSizeX1D - 1) / ThreadGroupSizeX1D;

            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(*resources_.predictedPosition, vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.particleCountPerCube, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::ComputeParticleCounts]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::ComputeParticleCounts], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }

        // Apply Block Offsets
        {
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(storageBuffers_.cubeOffset, vks::BufferUsage::ComputeShaderReadWrite);
            
            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::SumCubeOffsets]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::SumCubeOffsets], descriptorSet_);
            commandList.dispatch(1, 1, 1);
        }

        // Assign Particles
        {
            uint32_t groupCountX = (ParticleCount + ThreadGroupSizeX1D - 1) / ThreadGroupSizeX1D;

            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(*resources_.predictedPosition, vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.cubeOffset, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.particleIndexInCube, vks::BufferUsage::ComputeShaderReadWrite);
            
            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::AssignParticles]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::AssignParticles], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }

        // Perform Neighbor Search
        {
            uint32_t groupCountX = (ParticleCount + ThreadGroupSizeX1D - 1) / ThreadGroupSizeX1D;

            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(storageBuffers_.particleCountPerCube, vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.particleIndexInCube, vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(*resources_.predictedPosition, vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.neighborIndex, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.neighborCount, vks::BufferUsage::ComputeShaderReadWrite);
            
            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::PerformNeighborSearch]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::PerformNeighborSearch], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }
    }

    void HashGrid::prepareUniformBuffer()
    {
        context_->createUniformBuffer(uniformBuffer_);
        uniformBuffer_.map();

        uniformBuffer_.data.invSupportRadius = 1.f / SupportRadius;
        uniformBuffer_.data.supportRadius = SupportRadius;
        uniformBuffer_.data.domainSizeXZ = DomainSizeXZ;
        uniformBuffer_.data.cubeCountXZ = CubeCountXZ;
        uniformBuffer_.data.cubeCountY = CubeCountY;
        uniformBuffer_.data.cubeCount = CubeCount;
        uniformBuffer_.data.particleCount = ParticleCount;
        uniformBuffer_.data.maxNeighborCount = MaxNeighborCount;
        
        uniformBuffer_.update();
    }

    void HashGrid::createStorageBuffers()
    {
        context_->createDeviceBuffer(
            storageBuffers_.particleCountPerCube, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            CubeCount);

        context_->createDeviceBuffer(
            storageBuffers_.cubeOffset,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            CubeCount);
        
        context_->createDeviceBuffer(
            storageBuffers_.particleIndexInCube,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ParticleCount);

        context_->createDeviceBuffer(
            storageBuffers_.neighborCount, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
            ParticleCount);

        context_->createDeviceBuffer(
            storageBuffers_.neighborIndex, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
            ParticleCount * MaxNeighborCount);
    }
    
    void HashGrid::setupDescriptors()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void HashGrid::preparePipelines()
    {
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        VkPipelineLayout pipelineLayout = context_->createPipelineLayout(pipelineLayoutInfo);

        VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

        // Create shader modules
        constexpr const char* shaderNames[] = {
            "computeParticleCountsCS",
            "sumCubeOffsetsCS",
            "assignParticlesCS",
            "performNeighborSearchCS"
        };

        constexpr const char* shaderPaths[] = {
            "ComputeParticleCounts.comp.spv",
            "SumCubeOffsets.comp.spv",
            "AssignParticles.comp.spv",
            "PerformNeighborSearch.comp.spv"
        };

        std::string shaderBasePath = getShaderBasePath() + "Simulator/NeighborSearch/";
        utils::EnumArray<vks::ShaderModule, PipelineID> shaderModules;

        auto createPipeline = [&](PipelineID id)
        {
            uint32_t index = static_cast<uint32_t>(id);
            vks::AssetLoader::createShaderModule(
                *context_, 
                shaderModules[id], 
                shaderBasePath + shaderPaths[index],
                shaderNames[index]);

            pipelineCreateInfo.stage = vks::pipelineShaderStageCreateInfo(shaderModules[id], VK_SHADER_STAGE_COMPUTE_BIT, shaderNames[index]);
            context_->createComputePipeline(pipelines_[id], pipelineCreateInfo);
        };

        utils::forEachEnum<PipelineID>(createPipeline);
    }
}