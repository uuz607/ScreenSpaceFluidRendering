#pragma once

#include "vulkan/vulkan.h"
#include <vector>
#include "VulkanTexture.h"
#include <span>

namespace vks 
{
	class VulkanContext;

	/**
	* Attachments
	*
	* Manages color and depth attachments for a render pass.
	*/
	class Attachments
	{
    public:
		Attachments() = default;
		~Attachments() = default;

		void addColorAttachment(Texture* colorAttachment);
		void addDepthAttachment(Texture* depthAttachment);

		uint32_t getColorAttachmentCount() const;
		uint32_t getAttachmentCount() const;

		bool hasDepthAttachment() const { return depthAttachment_.texture? true : false; }

		std::vector<VkAttachmentReference> getColorReferences() const;
		VkAttachmentReference getDepthReference() const;

		std::vector<VkImageView> getAttachementViews() const;

		VkExtent2D getExtent() const;

		void setFinalLayouts(std::span<const VkAttachmentDescription> attachmentDescs);

	public:
		struct Attachment
		{
			Texture* texture = nullptr;
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		};

		std::span<const Attachment> colorAttachments() const { return colorAttachments_; }
		const Attachment& depthAttachment() const { return depthAttachment_; }

    private:
		std::vector<Attachment> colorAttachments_;
		Attachment depthAttachment_;
	};

	inline VkAttachmentDescription makeDefaultDescription(const Texture& texture, TextureUsage srcUsage, TextureUsage dstUsage)
	{
		VkAttachmentDescription attachmentDesc = {
			0,
			texture.format(),
			VK_SAMPLE_COUNT_1_BIT,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			vks::getDefaultLayout(srcUsage),
			vks::getDefaultLayout(dstUsage)
		};

		return attachmentDesc;
	}


	/**
	*	Render pass
	*/
	class RenderPass
	{
	public:
		RenderPass() = default;
		~RenderPass();

		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;
		
		VkRenderPass handle() const { return renderPass_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkRenderPass renderPass_ = VK_NULL_HANDLE;

		friend class VulkanContext;
	};


	/**
	*	Frame buffer
	*/
	class Framebuffer
	{
	public:
		Framebuffer() = default;
		~Framebuffer();

		Framebuffer(const Framebuffer&) = delete;
		Framebuffer& operator=(const Framebuffer&) = delete;

		Framebuffer(Framebuffer&& other) noexcept;
        Framebuffer& operator=(Framebuffer&& other) noexcept;

		VkFramebuffer handle() const { return framebuffer_; }

	private:
	
	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
		
		friend class VulkanContext;
	};
}