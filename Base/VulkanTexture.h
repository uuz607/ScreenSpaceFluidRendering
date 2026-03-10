#pragma once

#include <vulkan/vulkan.h>
#include "VulkanSampler.h"

namespace vks
{
	enum class TextureType : uint8_t
	{
		Texture1D,
		Texture2D,
		Texture2DArray,
		TextureCube,
		TextureCubeArray,
		Texture3D
	};

	enum class TextureUsage : uint32_t
	{
		Undefined,
		TransferSrc,
		TransferDst,
		ColorAttachmentWrite,
		DepthAttachmentWrite,
		DepthAttachmentRead,
		ShaderSampledRead,
		ShaderStorageWrite,
		Present,
		ComputeShaderRead,
		ComputeShaderWrite
	};
	
	struct TextureCreateInfo
	{
		TextureType type = TextureType::Texture2D;
		VkFormat format;
		VkImageUsageFlags usage;
		uint32_t width;
		uint32_t height;
		uint32_t levelCount = 1;
		uint32_t layerCount = 1;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
	};


	class Texture
	{
	public:
		Texture() = default;
		~Texture();

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;

		VkImage image() const { return image_; }
		VkImageView imageView()   const { return imageView_; }
		VkFormat format() const { return format_; }
		VkImageAspectFlags aspectMask() const { return aspectMask_; }
		
		uint32_t width()  const { return width_; }
		uint32_t height() const { return height_; }

		uint32_t levelCount() const { return levelCount_; }
		uint32_t layerCount() const { return layerCount_; }

		VkImageLayout getCurrentLayout() const { return currentImageLayout_; }
		void setCurrentLayout(VkImageLayout imageLayout) { currentImageLayout_ = imageLayout; }

		TextureUsage getCurrentUsage() const { return currentImageUsage_; };
		void setCurrentUsage(TextureUsage imageUsage) { currentImageUsage_ = imageUsage; }

	private:
		void destroy();

	private:
		VkDevice device_ = VK_NULL_HANDLE;

		VkImage image_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkImageView imageView_ = VK_NULL_HANDLE;
		VkFormat format_ = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags aspectMask_ = VK_IMAGE_ASPECT_NONE;

		uint32_t width_ = 0;
		uint32_t height_ = 0;
		uint32_t levelCount_ = 0;
		uint32_t layerCount_ = 0;

		VkImageLayout currentImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
		TextureUsage currentImageUsage_ = TextureUsage::Undefined;

		bool isSwapchainImage_ = false;

		friend class VulkanContext;
		friend class VulkanSwapChain;
	};

	inline VkImageLayout getDefaultLayout(TextureUsage usage)
    {
        switch (usage) 
        {
		case TextureUsage::TransferSrc:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		case TextureUsage::TransferDst:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        case TextureUsage::ColorAttachmentWrite:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        case TextureUsage::DepthAttachmentWrite:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        case TextureUsage::DepthAttachmentRead:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        case TextureUsage::ShaderSampledRead:
		case TextureUsage::ComputeShaderRead:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		case TextureUsage::ShaderStorageWrite:
		case TextureUsage::ComputeShaderWrite:
			return VK_IMAGE_LAYOUT_GENERAL;
			
		case TextureUsage::Present:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        default:
            return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

	inline VkDescriptorImageInfo descriptorImageInfo(
		const Texture& texture, 
		TextureUsage usage,
		const Sampler* sampler = nullptr)
	{
		VkDescriptorImageInfo imageInfo;
		imageInfo.sampler = sampler? sampler->handle() : VK_NULL_HANDLE;
		imageInfo.imageView = texture.imageView();
		imageInfo.imageLayout = vks::getDefaultLayout(usage);

		return imageInfo;
	}
}