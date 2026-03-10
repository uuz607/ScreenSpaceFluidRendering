#include "PBFSolver.h"
#include "PBFKernel.h"
#include "VulkanContext.h"
#include "VulkanShader.hpp"
#include "AssetLoader.h"
#include "VulkanCommand.h"
#include "Simulator/Parameters.h"
#include "NeighborSearch/HashGrid.h"
#include "VulkanTools.h"

namespace ssfr
{
    inline constexpr uint32_t SolverIterations = 4;
    inline constexpr float LambdaEps = 1.0e-6f;
    inline constexpr float XSPHCoeff = 0.005f;
    inline constexpr float VorticityCoeff = 0.05f;

    void PBFSolver::create(vks::VulkanContext* context, PBFKernel* kernel, HashGrid* neighborSearch)
    {
        assert(kernel && neighborSearch);
        context_ = context;
        kernel_ = kernel;
        neighborSearch_ = neighborSearch;

        prepareUniformBuffer();
        createStorageBuffers();
        setupDescriptors();
        preparePipelines();

        HashGridResources resources = { &storageBuffers_.predictedPosition };
        neighborSearch_->setResources(resources);
    }

    void PBFSolver::setResources(const PBFSolverResources& resources)
    {
        resources_ = resources;

        // Update descriptor set
        VkDescriptorBufferInfo uniformDescInfo = uniformBuffer_.descriptorInfo();
        VkDescriptorBufferInfo posDescInfo = resources_.position->descriptorInfo();
        VkDescriptorBufferInfo predictedPosDescInfo = storageBuffers_.predictedPosition.descriptorInfo();
        VkDescriptorBufferInfo velocityDescInfo = resources_.velocity->descriptorInfo();
        VkDescriptorBufferInfo vorticityDescInfo = storageBuffers_.vorticity.descriptorInfo();
        VkDescriptorBufferInfo deltaPosDescInfo = storageBuffers_.deltaPosition.descriptorInfo();
        VkDescriptorBufferInfo lambdaDescInfo = storageBuffers_.lambda.descriptorInfo();
        VkDescriptorBufferInfo neighborCountDescInfo = neighborSearch_->getNeighborCountDescriptor();
        VkDescriptorBufferInfo neighborIndexDescInfo = neighborSearch_->getNeighborIndexDescriptor();
        VkDescriptorImageInfo kernelDescInfo = kernel_->getDescriptor(vks::TextureUsage::ComputeShaderRead);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &posDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &predictedPosDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &velocityDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &vorticityDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &deltaPosDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &lambdaDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &neighborCountDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8, &neighborIndexDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 9, &kernelDescInfo),
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void PBFSolver::step(vks::CommandList& commandList)
    {
        // Record
        vks::CommandContext commandContext;

        uint32_t groupCountX = (ParticleCount + ThreadGroupSizeX1D - 1) / ThreadGroupSizeX1D;

        // Predict position
        {
            // Acquire barrier
            uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
            uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
            if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
            {
                commandContext.acquireBuffer(*resources_.position, vks::BufferUsage::ComputeShaderRead, srcQueueFamilyIndex, dstQueueFamilyIndex);
            }
            else 
            {
                commandContext.useBuffer(*resources_.position, vks::BufferUsage::ComputeShaderRead);
            }

            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(*resources_.velocity, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::PredictPosition]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::PredictPosition], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }

        // Neighbor search
        neighborSearch_->execute(commandList);

        // Enforce constraint
        for (uint32_t i = 0; i < SolverIterations; i++)
        {
            // Compute lambda
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(neighborSearch_->neighborCountBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(neighborSearch_->neighborIndexBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useTexture(kernel_->texture(), vks::TextureUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.lambda, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::ComputeLambda]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::ComputeLambda], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);

            // Compute delta position
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(neighborSearch_->neighborCountBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(neighborSearch_->neighborIndexBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.lambda, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useTexture(kernel_->texture(), vks::TextureUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.deltaPosition, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::ComputeDeltaPosition]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::ComputeDeltaPosition], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);

            // Handle boundary collision
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.deltaPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(*resources_.velocity, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::HandleBoundaryCollision]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::HandleBoundaryCollision], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);

            // Adjust predicted position
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(storageBuffers_.deltaPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(storageBuffers_.vorticity, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::AdjustPredictedPosition]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::AdjustPredictedPosition], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }

        // update particle
        {
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderReadWrite);
            commandContext.useBuffer(*resources_.velocity, vks::BufferUsage::ComputeShaderWrite);
            commandContext.useBuffer(*resources_.position, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::UpdateParticle]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::UpdateParticle], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);

            // Release barrier
            uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
            uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
            if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
            {
                commandContext.releaseBuffer(*resources_.position, vks::BufferUsage::ComputeShaderReadWrite, srcQueueFamilyIndex, dstQueueFamilyIndex);
            }
            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();
        }

        // XSPH
        {
            commandContext.useBuffer(uniformBuffer_, vks::BufferUsage::ComputeUniformRead);
            commandContext.useBuffer(neighborSearch_->neighborCountBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(neighborSearch_->neighborIndexBuffer(), vks::BufferUsage::ComputeShaderRead);
            commandContext.useBuffer(storageBuffers_.predictedPosition, vks::BufferUsage::ComputeShaderRead);
            commandContext.useTexture(kernel_->texture(), vks::TextureUsage::ComputeShaderRead);
            commandContext.useBuffer(*resources_.velocity, vks::BufferUsage::ComputeShaderReadWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindPipeline(pipelines_[PipelineID::XSPH]);
            commandList.bindDescriptorSets(pipelines_[PipelineID::XSPH], descriptorSet_);
            commandList.dispatch(groupCountX, 1, 1);
        }
    }

    void PBFSolver::prepareUniformBuffer()
    {
        context_->createUniformBuffer(uniformBuffer_);
        uniformBuffer_.map();

        uniformBuffer_.data.gravity = glm::vec4(Gravity, 0.f);
        uniformBuffer_.data.supportRadius = SupportRadius;
        uniformBuffer_.data.volume0 = Volume0;
        uniformBuffer_.data.invRho0 = 1.f / Density0;
        uniformBuffer_.data.lambdaEps = LambdaEps;
        uniformBuffer_.data.domainSizeXZ = DomainSizeXZ;
        uniformBuffer_.data.domainSizeY = DomainSizeY;
        uniformBuffer_.data.dt = Dt;
        uniformBuffer_.data.invDt = 1.f / Dt;
        uniformBuffer_.data.xsphCoeff = XSPHCoeff;
        uniformBuffer_.data.vorticityCoeff = VorticityCoeff;
        uniformBuffer_.data.particleCount = ParticleCount;
        uniformBuffer_.data.maxNeighborCount = MaxNeighborCount; 

        uniformBuffer_.update();
    }

    void PBFSolver::createStorageBuffers()
    {
        context_->createDeviceBuffer(
            storageBuffers_.predictedPosition,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ParticleCount);
        
        context_->createDeviceBuffer(
            storageBuffers_.vorticity,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ParticleCount);

        context_->createDeviceBuffer(
            storageBuffers_.deltaPosition, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            ParticleCount);

        context_->createDeviceBuffer(
            storageBuffers_.lambda, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            ParticleCount);
    }

    void PBFSolver::setupDescriptors()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 9)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void PBFSolver::preparePipelines()
    {
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        VkPipelineLayout pipelineLayout = context_->createPipelineLayout(pipelineLayoutInfo);

        VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

        // Create shader modules
        constexpr const char* shaderNames[] = {
            "predictPositionCS",
            "computeLambdaCS",
            "computeDeltaPositionCS",
            "handleBoundaryCollisionCS",
            "adjustPredictedPositionCS",
            "updateParticleCS",
            "xsphCS"
        };

        constexpr const char* shaderPaths[] = {
            "PredictPosition.comp.spv",
            "ComputeLambda.comp.spv",
            "ComputeDeltaPosition.comp.spv",
            "HandleBoundaryCollision.comp.spv",
            "AdjustPredictedPosition.comp.spv",
            "UpdateParticle.comp.spv",
            "XSPH.comp.spv"
        };

        std::string shaderBasePath = getShaderBasePath() + "Simulator/FluidSolver/";
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