#include "Simulator.h"
#include "VulkanContext.h"
#include "VulkanCommand.h"
#include "FluidSolver/PBFSolver.h"
#include "NeighborSearch/HashGrid.h"
#include "Parameters.h"

namespace ssfr 
{
    Simulator::Simulator() : pbfKernel_(SupportRadius) {}

    void Simulator::create(vks::VulkanContext* context)
    {
        context_ = context;

        prepareStorageBuffers();

        // Initialize spatial neighbor search structure (e.g., uniform grid)
        // for particle neighborhood queries on GPU.
        neighborSearch_.create(context_);

        // Generate precomputed PBF kernel data (e.g., smoothing kernel lookup table)
        // with a resolution of 2048 samples.
        pbfKernel_.generate(*context_, 2048);

        // Create Position-Based Fluids solver, attaching kernel and neighbor search modules.
        // This sets up compute pipelines and internal solver buffers.
        pbfSolver_.create(context_, &pbfKernel_, &neighborSearch_);

        // Bind particle state buffers (position & velocity) as solver input/output resources.
        PBFSolverResources resources = { &particlePosition_, &particleVelocity_ };
        pbfSolver_.setResources(resources);
    }

    void Simulator::run(vks::CommandList& commandList)
    {
        pbfSolver_.step(commandList);   
    }

    void Simulator::prepareStorageBuffers()
    {
        std::vector<glm::vec4> positions(ParticleCount);

		constexpr float diameter = 2.f * ParticleRadius;

        //One Dam Break
        uint32_t index = 0;
        float x = -0.3f * DomainSizeXZ + 4.f * diameter;
        for (unsigned int i = 0; i < ParticleCountXZ; i++) 
        {
            float y = 20.f * diameter;
            for (unsigned int j = 0; j < ParticleCountY; j++) 
            {
                float z = -0.3f * DomainSizeXZ + 4.f * diameter;
                for (unsigned int k = 0; k < ParticleCountXZ; k++) 
                {
                    positions[index++] = glm::vec4(x, y, z, 0.f);
                    z += diameter;
                }
                y += diameter;
            }
            x += diameter;
        }

        // Create storage buffers
        context_->createDeviceBuffer(
            particlePosition_, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            ParticleCount,
            positions.data());

        context_->createDeviceBuffer(
            particleVelocity_, 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            ParticleCount);

        // Fill velocity 
        vks::CommandContext commandContext;
        vks::CommandList commandList;

        context_->createCommandList(commandList, true);

        commandContext.useBuffer(particleVelocity_, vks::BufferUsage::TransferDst);
        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();
        
        commandList.fillBuffer(particleVelocity_, 0);

        // Execute a transfer barrier to the compute queue, if necessary
        uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
        uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
        if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
        {
            commandContext.releaseBuffer(particlePosition_, vks::BufferUsage::ShaderRead, srcQueueFamilyIndex, dstQueueFamilyIndex);
        }

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        context_->flushCommandList(commandList);
    }
}