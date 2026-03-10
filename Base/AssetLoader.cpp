#include "AssetLoader.h"
#include "VulkanglTFModel.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"
#include "VulkanShader.hpp"
#include "VulkanCommand.h"
#include "VulkanTools.h"

namespace vks
{
    void AssetLoader::loadGlTFModel(
        VulkanContext& context,
        vkglTF::Model& model,
        uint32_t glTFLoadingFlags,
        const std::string& assetPath)
    {
        VulkanDevice* device = context.vulkanDevice();
        VkQueue transferQueue = context.transferQueue();

        model.loadFromFile(assetPath, device, transferQueue);
    }

    void AssetLoader::loadKtxTexture(
        VulkanContext& context,
        Texture& outTexture,
        TextureType type,
        VkFormat format,
        const std::string& filename,
        VkImageUsageFlags imageUsageFlags)
    {
		ktxTexture* ktxTexture;

		if(!tools::fileExists(filename))
		{
			tools::exitFatal("Could not load Texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
		}
		ktxResult result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		assert(result == KTX_SUCCESS);

		TextureCreateInfo textureInfo;
		textureInfo.type = type;
        textureInfo.format = format;
		textureInfo.usage = imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		textureInfo.width = ktxTexture->baseWidth;
		textureInfo.height = ktxTexture->baseHeight;
		textureInfo.levelCount = ktxTexture->numLevels;
		textureInfo.layerCount = ktxTexture->numLayers * ktxTexture->numFaces;

		ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTexture);
		ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);

		// Create a host-visible staging buffer that contains the raw image data
        vks::Buffer stagingBuffer;
        context.createBuffer(
            stagingBuffer, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            ktxTextureSize, 
            ktxTextureData);

		// Setup buffer copy regions for each face including all of its mip levels
		std::vector<VkBufferImageCopy> bufferCopyRegions;

		for(uint32_t layer = 0; layer < textureInfo.layerCount; ++layer)
		{
			for(uint32_t level = 0; level < textureInfo.levelCount; ++level)
			{
				ktx_size_t offset;
				KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, layer, &offset);
				assert(result == KTX_SUCCESS);

				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = level;
				bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
				bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
				bufferCopyRegion.imageExtent.depth = 1;
				bufferCopyRegion.bufferOffset = offset;

				bufferCopyRegions.push_back(bufferCopyRegion);
			}
		}

		// Create optimal tiled target image
		context.createTexture(outTexture, textureInfo);

		// Use a separate command buffer for Texture loading
		CommandList commandList;
		context.createCommandList(commandList, true);
		
		// Image barrier for optimal image (target)
		CommandContext commandContext;
		commandContext.useTexture(outTexture, vks::TextureUsage::TransferDst);

		commandContext.flushBarriers(commandList);
		commandContext.commitTransitions();

		// Copy mip levels from staging buffer
		commandList.copyBufferToImage(stagingBuffer, outTexture, bufferCopyRegions);
		context.flushCommandList(commandList);

		// Clean up staging resources
		ktxTexture_Destroy(ktxTexture);
    }

	void AssetLoader::createShaderModule(
		VulkanContext& context,
		ShaderModule& outShaderModule, 
		const std::string& shaderPath, 
		const char* entryPoint)
	{
		outShaderModule.device_ = context.device();

        VkShaderModule shaderModule = tools::loadShader(shaderPath.c_str(), context.device());
        assert(shaderModule != VK_NULL_HANDLE);
        outShaderModule.shaderModule_ = shaderModule;
	}
}