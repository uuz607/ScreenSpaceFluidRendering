#include "PBFKernel.h"
#include "VulkanContext.h"
#include "VulkanCommand.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace ssfr
{
    PBFKernel::PBFKernel(float supportRadius) : h_(supportRadius) 
    {
        constexpr float pi = glm::pi<float>();

        // cubic spline kernel coefficient
        wcoeff_ = 315.f / (64.f * pi * pow(h_, 9.f));

        // Spiky kernel gradient coefficient
        dwcoeff_ = -45.f / (pi * pow(h_, 6.f));
    }

    VkDescriptorImageInfo PBFKernel::getDescriptor(vks::TextureUsage usage) const
    {
        return vks::descriptorImageInfo(kernelTexture_, usage, &kernelSampler_);
    }

    void PBFKernel::generate(vks::VulkanContext& context, uint32_t width)
    {
        std::vector<glm::vec2> buffer(width);

        for (uint32_t i = 0; i < width; ++i)
        {
            float r = (i + 0.5f) * h_/ width;  // [0, h]
            buffer[i].r = poly6W(r);
            buffer[i].g = spikyGradW(r);
        }

        // Create a host-visible staging buffer that contains the raw image data
        vks::Buffer stagingBuffer;
        context.createBuffer(
            stagingBuffer, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            buffer.size() * sizeof(glm::vec2), 
            buffer.data());
        
        vks::TextureCreateInfo textureInfo;
        textureInfo.type = vks::TextureType::Texture1D;
        textureInfo.format = VK_FORMAT_R32G32_SFLOAT;
        textureInfo.usage =  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        textureInfo.width = width;
        textureInfo.height = 1;
        context.createTexture(kernelTexture_, textureInfo);
        
        // Copy region
        VkBufferImageCopy bufferCopyRegion[1] = {};
        bufferCopyRegion[0].imageSubresource.aspectMask = kernelTexture_.aspectMask();
        bufferCopyRegion[0].imageSubresource.mipLevel = 0;
        bufferCopyRegion[0].imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion[0].imageSubresource.layerCount = 1;
        bufferCopyRegion[0].imageExtent.width = kernelTexture_.width();
        bufferCopyRegion[0].imageExtent.height = kernelTexture_.height();
        bufferCopyRegion[0].imageExtent.depth = 1;

        // Copy image
        vks::CommandList commandList;
        context.createCommandList(commandList, true);

        vks::CommandContext commandContext;
        commandContext.useTexture(kernelTexture_, vks::TextureUsage::TransferDst);
        
        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        commandList.copyBufferToImage(stagingBuffer, kernelTexture_, bufferCopyRegion);
        context.flushCommandList(commandList);

        // Create sampler
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        context.createSampler(kernelSampler_, samplerInfo);
    }

    float PBFKernel::poly6W(float r)
    {
        float val = 0.f;
        float r2 = r * r;
        float h2 = h_ * h_;
        if (r2 <= h2) val = pow(h2 - r2, 3.f) * wcoeff_;
        return val;
    }

    float PBFKernel::spikyGradW(float r)
    {
        float val = 0.f;
        float r2 = r * r;
        float h2 = h_ * h_;
        if (r2 <= h2)
        {
            float hr = h_ - r;
            float hr2 = hr * hr;
            val = dwcoeff_ * hr2 / r;
        }
        return val;
    }
}