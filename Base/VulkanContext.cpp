#include "VulkanContext.h"
#include "VulkanSampler.h"
#include "VulkanDescriptorSet.h"
#include "VulkanCommand.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"
#include "VulkanRenderPass.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"

#include <ktx.h>
#include <ktxvulkan.h>

namespace vks 
{
    inline VkImageViewType getImageViewType(TextureType type)
	{
		switch (type) 
		{
		case TextureType::Texture1D:
			return VK_IMAGE_VIEW_TYPE_1D;

		case TextureType::Texture2D:
        	return VK_IMAGE_VIEW_TYPE_2D;

		case TextureType::Texture2DArray:
			return VK_IMAGE_VIEW_TYPE_2D_ARRAY;

		case TextureType::TextureCube:
			return VK_IMAGE_VIEW_TYPE_CUBE;

		case TextureType::TextureCubeArray:
			return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

		case TextureType::Texture3D:
			return VK_IMAGE_VIEW_TYPE_3D;

		default:
			assert(false && "Unsupported TextureType in getImageViewType");
			return VK_IMAGE_VIEW_TYPE_2D;
		}
	}

	inline VkImageType getImageType(TextureType type)
	{
		switch (type) 
		{
		case TextureType::Texture1D:
			return VK_IMAGE_TYPE_1D;

		case TextureType::Texture2D:
		case TextureType::Texture2DArray:
		case TextureType::TextureCube:
		case TextureType::TextureCubeArray:
			return VK_IMAGE_TYPE_2D;

		case TextureType::Texture3D:
			return VK_IMAGE_TYPE_3D;

		default:
			assert(false && "Unsupported TextureType in getImageType");
			return VK_IMAGE_TYPE_2D;
		}
	}


	VulkanContext::~VulkanContext()
	{
		delete samplerManager_;
		delete descriptorSetLayoutManager_;
		delete pipelineLayoutManager_;

		if(graphicsCommandPool_)
		{
			vkDestroyCommandPool(logicalDevice_, graphicsCommandPool_, nullptr);
		}
		if(computeCommandPool_)
		{
			vkDestroyCommandPool(logicalDevice_, computeCommandPool_, nullptr);
		}

		if (pipelineCache_) 
		{
			vkDestroyPipelineCache(logicalDevice_, pipelineCache_, nullptr);
		}

		if (descriptorPool_) 
		{
			vkDestroyDescriptorPool(logicalDevice_, descriptorPool_, nullptr);
		}
	}

	void VulkanContext::createContext(VulkanDevice* vulkanDevice)
	{
		vulkanDevice_ = vulkanDevice;
		logicalDevice_ = vulkanDevice->logicalDevice;

		samplerManager_ = new SamplerManager(logicalDevice_);
		descriptorSetLayoutManager_ = new DescriptorSetLayoutManager(logicalDevice_);
		pipelineLayoutManager_ = new PipelineLayoutManager(logicalDevice_);

		// Get device queue
		vkGetDeviceQueue(logicalDevice_, vulkanDevice->queueFamilyIndices.graphics, 0, &graphicsQueue_);
		vkGetDeviceQueue(logicalDevice_, vulkanDevice->queueFamilyIndices.compute, 0, &computeQueue_);

		// Find a suitable depth and/or stencil format
		VkBool32 validFormat = false;
		// Samples that make use of stencil will require a depth + stencil format, so we select from a different list
		if (requireStencil_) 
		{
			validFormat = vks::tools::getSupportedDepthStencilFormat(vulkanDevice->physicalDevice, &depthFormat_);
		} 
		else 
		{
			validFormat = vks::tools::getSupportedDepthFormat(vulkanDevice->physicalDevice, &depthFormat_);
		}
		assert(validFormat);

		// Create graphics and compute command pool
		VkCommandPoolCreateInfo commandPoolInfo = {};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice_, &commandPoolInfo, nullptr, &graphicsCommandPool_));

		commandPoolInfo.queueFamilyIndex = vulkanDevice_->queueFamilyIndices.compute;
		VK_CHECK_RESULT(vkCreateCommandPool(logicalDevice_, &commandPoolInfo, nullptr, &computeCommandPool_));

		// Create pipeline cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_RESULT(vkCreatePipelineCache(logicalDevice_, &pipelineCacheCreateInfo, nullptr, &pipelineCache_));

		// Create descriptor pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 20),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 20),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 20),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 30);
		descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		VK_CHECK_RESULT(vkCreateDescriptorPool(logicalDevice_, &descriptorPoolInfo, nullptr, &descriptorPool_));
	}

    void VulkanContext::createSampler(Sampler& sampler, const VkSamplerCreateInfo& samplerInfo)
    {
        samplerManager_->createSampler(sampler, samplerInfo);
    }

    void VulkanContext::createDescriptorSet(DescriptorSet& outDescriptorSet, std::span<VkDescriptorSetLayoutBinding> layoutBindings)
    {
		if(outDescriptorSet.descriptorSet_)
		{
            vkFreeDescriptorSets(logicalDevice_, descriptorPool_, 1, &outDescriptorSet.descriptorSet_);
			outDescriptorSet.descriptorSet_ = VK_NULL_HANDLE;
		}

        outDescriptorSet.device_ = logicalDevice_;
		outDescriptorSet.descriptorPool_ = descriptorPool_;

        VkDescriptorSetLayout descriptorSetLayout = descriptorSetLayoutManager_->createDescriptorSetLayout(layoutBindings);
        
        VkDescriptorSetAllocateInfo allocInfo = initializers::descriptorSetAllocateInfo(descriptorPool_, &descriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(logicalDevice_, &allocInfo, &outDescriptorSet.descriptorSet_));
        
        outDescriptorSet.layout_ = descriptorSetLayout;
    }

    void VulkanContext::createBuffer(
        Buffer& outBuffer, 
        VkBufferUsageFlags usageFlags, 
        VkMemoryPropertyFlags memoryPropertyFlags,
        VkDeviceSize size, 
        const void *data)
    {
		if (outBuffer.buffer_)
		{
			vkDestroyBuffer(logicalDevice_, outBuffer.buffer_, nullptr);
            outBuffer.buffer_ = VK_NULL_HANDLE;
		}
		
		if (outBuffer.memory_)
		{
			vkFreeMemory(logicalDevice_, outBuffer.memory_, nullptr);
            outBuffer.memory_ = VK_NULL_HANDLE;
		}

        VK_CHECK_RESULT(vulkanDevice_->createBuffer(usageFlags, memoryPropertyFlags, size, &outBuffer.buffer_, &outBuffer.memory_, data));
        
        outBuffer.device_ = logicalDevice_;
		outBuffer.usageFlags_ = usageFlags;
		outBuffer.memoryPropertyFlags_ = memoryPropertyFlags;
		outBuffer.size_ = size;
    }

    void VulkanContext::createTexture(Texture& outTexture, const TextureCreateInfo& textureInfo)
    {
		if (outTexture.imageView_) 
		{
			vkDestroyImageView(logicalDevice_, outTexture.imageView_, nullptr);
			outTexture.imageView_ = VK_NULL_HANDLE;
		}

		if (outTexture.image_)
		{
			if (!outTexture.isSwapchainImage_) 
			{
				vkDestroyImage(logicalDevice_, outTexture.image_, nullptr);
			}
			outTexture.image_ = VK_NULL_HANDLE;
		}

		if (outTexture.memory_) 
		{
			vkFreeMemory(logicalDevice_, outTexture.memory_, nullptr);
			outTexture.memory_ = VK_NULL_HANDLE;
		}

		VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo();
		imageCreateInfo.imageType = vks::getImageType(textureInfo.type);
		imageCreateInfo.format = textureInfo.format;
		imageCreateInfo.extent.width = textureInfo.width;
		imageCreateInfo.extent.height = textureInfo.height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = textureInfo.levelCount;
		imageCreateInfo.arrayLayers = textureInfo.layerCount;
		imageCreateInfo.samples = textureInfo.samples;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = textureInfo.usage;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (textureInfo.type == TextureType::TextureCube)
		{
			imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		VkMemoryAllocateInfo memAlloc = initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(logicalDevice_, &imageCreateInfo, nullptr, &outTexture.image_));
		vkGetImageMemoryRequirements(logicalDevice_, outTexture.image_, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice_->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(logicalDevice_, &memAlloc, nullptr, &outTexture.memory_));
		VK_CHECK_RESULT(vkBindImageMemory(logicalDevice_, outTexture.image_, outTexture.memory_, 0));

        outTexture.currentImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        outTexture.currentImageUsage_ = TextureUsage::Undefined;

		VkImageAspectFlags aspectMask = tools::getAspectFromFormat(textureInfo.format);

		VkImageViewCreateInfo viewCreateInfo = initializers::imageViewCreateInfo();
		viewCreateInfo.viewType = vks::getImageViewType(textureInfo.type);
		viewCreateInfo.format = textureInfo.format;
		viewCreateInfo.subresourceRange = { aspectMask, 0, textureInfo.levelCount, 0, textureInfo.layerCount };
		viewCreateInfo.image = outTexture.image_;
		VK_CHECK_RESULT(vkCreateImageView(logicalDevice_, &viewCreateInfo, nullptr, &outTexture.imageView_));

		outTexture.device_ = logicalDevice_;
		outTexture.aspectMask_ = aspectMask;
        outTexture.format_ = textureInfo.format;
		outTexture.width_ = textureInfo.width;
		outTexture.height_ = textureInfo.height;
		outTexture.levelCount_ = textureInfo.levelCount;
		outTexture.layerCount_ = textureInfo.layerCount;
    }

    void VulkanContext::createRenderPass(RenderPass& outRenderPass, const VkRenderPassCreateInfo& renderPassInfo)
    {   
		if (outRenderPass.renderPass_)
        {
            vkDestroyRenderPass(logicalDevice_, outRenderPass.renderPass_, nullptr);
			outRenderPass.renderPass_ = VK_NULL_HANDLE;
        }

        VK_CHECK_RESULT(vkCreateRenderPass(logicalDevice_, &renderPassInfo, nullptr, &outRenderPass.renderPass_));
		outRenderPass.device_ = logicalDevice_;
    }

	void VulkanContext::createRenderPass(
		RenderPass& outRenderPass, 
		const Attachments& attachments,
		std::span<const VkAttachmentDescription> attachmentDescs)
	{
		std::vector<VkAttachmentReference> colorReferences = attachments.getColorReferences();
        VkAttachmentReference depthReference = attachments.getDepthReference();

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pColorAttachments = colorReferences.data();
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
        subpass.pDepthStencilAttachment = attachments.hasDepthAttachment()? &depthReference : nullptr;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pAttachments = attachmentDescs.data();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.subpassCount = 1;

		createRenderPass(outRenderPass, renderPassInfo);
	}

    void VulkanContext::createFramebuffer(Framebuffer& outFrameBuffer, const VkFramebufferCreateInfo& framebufferInfo)
    {
		if (outFrameBuffer.framebuffer_)
        {
            vkDestroyFramebuffer(logicalDevice_, outFrameBuffer.framebuffer_, nullptr);
			outFrameBuffer.framebuffer_ = VK_NULL_HANDLE;
        }

        VK_CHECK_RESULT(vkCreateFramebuffer(logicalDevice_, &framebufferInfo, nullptr, &outFrameBuffer.framebuffer_));
        outFrameBuffer.device_ = logicalDevice_;
    }

	void VulkanContext::createFramebuffer(Framebuffer& outFrameBuffer, const RenderPass& renderPass, const Attachments& attachments)
	{
		VkExtent2D extent = attachments.getExtent();
        // Get default attachment views
        std::vector<VkImageView> attachmentViews = attachments.getAttachementViews();

        VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
        framebufferInfo.renderPass = renderPass.handle();
        framebufferInfo.pAttachments = attachmentViews.data();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        createFramebuffer(outFrameBuffer, framebufferInfo);
	}

    void VulkanContext::createGraphicsPipeline(Pipeline& outPipeline, const VkGraphicsPipelineCreateInfo& pipelineCreateInfo)
    {
		if (outPipeline.pipeline_) 
		{
			vkDestroyPipeline(logicalDevice_, outPipeline.pipeline_, nullptr); 
		    outPipeline.pipeline_ = VK_NULL_HANDLE;
		}

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(
            logicalDevice_,
            pipelineCache_, 
            1, 
            &pipelineCreateInfo, 
            nullptr, 
            &outPipeline.pipeline_));

		outPipeline.device_ = logicalDevice_;	
        outPipeline.layout_ = pipelineCreateInfo.layout;
        outPipeline.bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

    void VulkanContext::createComputePipeline(Pipeline& outPipeline, const VkComputePipelineCreateInfo& pipelineCreateInfo)
    {
		if (outPipeline.pipeline_) 
		{
			vkDestroyPipeline(logicalDevice_, outPipeline.pipeline_, nullptr); 
		   	outPipeline.pipeline_ = VK_NULL_HANDLE;
		}

        VK_CHECK_RESULT(vkCreateComputePipelines(
            logicalDevice_,
            pipelineCache_, 
            1, 
            &pipelineCreateInfo, 
            nullptr, 
            &outPipeline.pipeline_));

		outPipeline.device_ = logicalDevice_;
        outPipeline.layout_ = pipelineCreateInfo.layout;
        outPipeline.bindPoint_ = VK_PIPELINE_BIND_POINT_COMPUTE;
    }

    void VulkanContext::createCommandList(
		CommandList& outCommandList,
		bool begin,
		QueueType queueType, 
		VkCommandBufferLevel level)
    {
		if (outCommandList.commandBuffer_) 
		{
			switch (outCommandList.queueType_) 
			{
			case QueueType::Graphics:
			case QueueType::Transfer:
				vkFreeCommandBuffers(logicalDevice_, graphicsCommandPool_, 1, &outCommandList.commandBuffer_);
				outCommandList.commandBuffer_ = VK_NULL_HANDLE;
				break;
			case QueueType::Compute:
				vkFreeCommandBuffers(logicalDevice_, computeCommandPool_, 1, &outCommandList.commandBuffer_);
				outCommandList.commandBuffer_ = VK_NULL_HANDLE;
				break;
			}			
		}

		outCommandList.queueType_ = queueType;

		switch (queueType) 
		{
		case QueueType::Graphics:
		case QueueType::Transfer:
			outCommandList.commandBuffer_ = vulkanDevice_->createCommandBuffer(level, graphicsCommandPool_, begin);
			break;

		case QueueType::Compute:
			outCommandList.commandBuffer_ = vulkanDevice_->createCommandBuffer(level, computeCommandPool_, begin);
			break;
		}
    }

    VkPipelineLayout VulkanContext::createPipelineLayout(const VkPipelineLayoutCreateInfo& pipelineLayoutInfo)
    {
        return pipelineLayoutManager_->createPipelineLayout(pipelineLayoutInfo);
    }

	void VulkanContext::flushCommandList(CommandList& commandList, bool free)
	{
		switch (commandList.queueType_) 
		{
		case QueueType::Graphics:
		case QueueType::Transfer:
			vulkanDevice_->flushCommandBuffer(commandList.commandBuffer_, graphicsQueue_, graphicsCommandPool_, free);
			break;
		
		case QueueType::Compute:
			vulkanDevice_->flushCommandBuffer(commandList.commandBuffer_, computeQueue_, computeCommandPool_, free);
			break;
		}

		if (free) 
		{
			commandList.commandBuffer_ = VK_NULL_HANDLE;
		}
	}
}