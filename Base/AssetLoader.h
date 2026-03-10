#pragma once

#include <stdint.h>
#include <string>
#include <vulkan/vulkan.h>


namespace vkglTF{
    class Model;
}

namespace vks
{
    class VulkanContext;
    class Texture;
    struct VulkanDevice;
    class ShaderModule;
    enum class TextureType : uint8_t;

    class AssetLoader
    {
    public:
        static void loadGlTFModel(
            VulkanContext& context,
            vkglTF::Model& model,
            uint32_t glTFLoadingFlags,
            const std::string& assetPath);
        
        static void loadKtxTexture(
            VulkanContext& context,
            Texture& outTexture,
            TextureType type,
            VkFormat format,
            const std::string& filename,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

        static void createShaderModule(
            VulkanContext& context,
            ShaderModule& outShaderModule, 
			const std::string& shaderPath, 
			const char* entryPoint = "main");
    };
}