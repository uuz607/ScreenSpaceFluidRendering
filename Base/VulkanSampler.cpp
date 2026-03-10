#include "VulkanSampler.h"
#include "VulkanTools.h"

namespace vks 
{
    size_t SamplerManager::SamplerKeyHash::operator()(const SamplerKey& key) const noexcept
    {
        size_t hash = 0;

        vks::tools::hashCombine(hash, key.magFilter);
        vks::tools::hashCombine(hash, key.minFilter);
        vks::tools::hashCombine(hash, key.addressMode);

        return hash;
    }

    SamplerManager::~SamplerManager()
    {
        for (const auto& [key, value] : samplerMap_)
        {
            vkDestroySampler(device_, value, nullptr);
        } 
    }

    void SamplerManager::createSampler(Sampler& outSampler, const VkSamplerCreateInfo& samplerInfo)
    {
        SamplerKey key = {
            samplerInfo.magFilter,
            samplerInfo.minFilter,
            samplerInfo.addressModeU
        };
        
        auto it = samplerMap_.find(key);
        if (it != samplerMap_.end()) 
        {
            outSampler.sampler_ = it->second;
            return;
        }

        VkSampler sampler;
        VK_CHECK_RESULT(vkCreateSampler(device_, &samplerInfo, nullptr, &sampler));
        samplerMap_.emplace(key, sampler);
        outSampler.sampler_ = sampler;
    }
}