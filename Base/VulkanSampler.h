#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>


namespace vks 
{
    inline VkSamplerCreateInfo samplerCreateInfo(VkFilter filter, VkSamplerAddressMode addressMode)
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = filter;
        samplerInfo.minFilter = filter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        return samplerInfo;
    }

    class Sampler
    {
    public:
        Sampler() = default;
        ~Sampler() = default;  

        VkSampler handle() const { return sampler_; }

    private:
        VkSampler sampler_ = VK_NULL_HANDLE;

        friend class SamplerManager;
    };

    class SamplerManager
    {
    public:
        explicit SamplerManager(VkDevice device) : device_(device) {}
        ~SamplerManager();

        SamplerManager(const SamplerManager&) = delete;
        SamplerManager& operator=(const SamplerManager&) = delete;    

        void createSampler(Sampler& outSampler, const VkSamplerCreateInfo& samplerInfo);

    private:
        VkDevice device_ = VK_NULL_HANDLE;

        struct SamplerKey
        {
            VkFilter             magFilter;
            VkFilter             minFilter;
            VkSamplerAddressMode addressMode;

            bool operator==(const SamplerKey&) const = default;
        };

        struct SamplerKeyHash
        {
            size_t operator()(const SamplerKey& key) const noexcept;
        };

        using SamplerMap = std::unordered_map<SamplerKey, VkSampler, SamplerKeyHash>;
        SamplerMap samplerMap_;
    };
}