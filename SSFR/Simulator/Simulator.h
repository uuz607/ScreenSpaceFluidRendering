#pragma once

#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>

#include "NeighborSearch/HashGrid.h"
#include "FluidSolver/PBFSolver.h"
#include "FluidSolver/PBFKernel.h"

namespace ssfr
{
    class Simulator
    {
    public:
        Simulator();
        ~Simulator() = default;

        vks::DeviceBuffer<glm::vec4>* particlePosition() { return &particlePosition_; }

        void create(vks::VulkanContext* context);
        void run(vks::CommandList& commandList);

    private:
        void prepareStorageBuffers();

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::DeviceBuffer<glm::vec4> particlePosition_;
        vks::DeviceBuffer<glm::vec4> particleVelocity_;

        HashGrid neighborSearch_;
        PBFKernel pbfKernel_;
        PBFSolver pbfSolver_;
    };
}