#pragma once

#include "VulkanDescriptorSet.h"
#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "EnumArray.hpp"

#include <glm/glm.hpp>

namespace vks {
    class CommandList;
}

namespace ssfr
{
    class PBFKernel;
    class HashGrid; 

    struct PBFSolverResources
    {
        vks::DeviceBuffer<glm::vec4>* position = nullptr;
        vks::DeviceBuffer<glm::vec4>* velocity = nullptr;
    };

    class PBFSolver
    {
    public:
        PBFSolver() = default;
        ~PBFSolver() = default;

        void create(vks::VulkanContext* context, PBFKernel* kernel, HashGrid* neighborSearch);
        void setResources(const PBFSolverResources& resources);
        void step(vks::CommandList& commandList);

    private:
        void prepareUniformBuffer();
        void createStorageBuffers();
        void setupDescriptors();
        void preparePipelines();

    private:
        struct StorageBuffers
        {
            vks::DeviceBuffer<glm::vec4> predictedPosition;
            vks::DeviceBuffer<glm::vec4> vorticity;
            vks::DeviceBuffer<glm::vec4> deltaPosition;
            vks::DeviceBuffer<float> lambda;
        };

        enum class PipelineID : uint8_t
        {
            PredictPosition,
            ComputeLambda,
            ComputeDeltaPosition,
            HandleBoundaryCollision,
            AdjustPredictedPosition,
            UpdateParticle,
            XSPH,
            Count
        };

        struct Params
        {
            glm::vec4 gravity;

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
            uint32_t particleCount;
            uint32_t maxNeighborCount;
        };

        using Pipelines = utils::EnumArray<vks::Pipeline, PipelineID>;

    private:
        vks::VulkanContext* context_ = nullptr;
        
        PBFKernel* kernel_ = nullptr;
        HashGrid* neighborSearch_ = nullptr;
        PBFSolverResources resources_;

        vks::UniformBuffer<Params> uniformBuffer_;
        StorageBuffers storageBuffers_;

        vks::DescriptorSet descriptorSet_;
        Pipelines pipelines_;
    };
}