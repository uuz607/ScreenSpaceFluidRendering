#pragma once

#include "VulkanTexture.h"
#include "VulkanSampler.h"

namespace vks { 
    class VulkanContext; 
}

namespace ssfr
{   
    class PBFKernel
    {
    public:
        explicit PBFKernel(float supportRadius);
        ~PBFKernel() = default;

        PBFKernel(const PBFKernel&) = delete;
        PBFKernel& operator=(const PBFKernel&) = delete;

        vks::Texture& texture() { return kernelTexture_; }
        VkDescriptorImageInfo getDescriptor(vks::TextureUsage usage) const;

        void generate(vks::VulkanContext& context, uint32_t width = 1024);

    private:
        float poly6W(float r);
        float spikyGradW(float r);

    private:
        float h_;
        float wcoeff_;
        float dwcoeff_;

        vks::Texture kernelTexture_;
        vks::Sampler kernelSampler_;
    };
}