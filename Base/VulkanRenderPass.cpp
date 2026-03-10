#include "VulkanRenderPass.h"
#include "VulkanContext.h"
#include "VulkanInitializers.hpp"

namespace vks 
{
    void Attachments::addColorAttachment(Texture* colorAttachment)
    {
        colorAttachments_.push_back({ colorAttachment, VK_IMAGE_LAYOUT_UNDEFINED });
    }

    void Attachments::addDepthAttachment(Texture* depthAttachment)
    {
        depthAttachment_ = { depthAttachment, VK_IMAGE_LAYOUT_UNDEFINED };
    }

    uint32_t Attachments::getColorAttachmentCount() const
    {
        return static_cast<uint32_t>(colorAttachments_.size());
    }

    uint32_t Attachments::getAttachmentCount() const 
    { 
        uint32_t count = getColorAttachmentCount();
        if (depthAttachment_.texture) ++count;
        return count; 
    }

    std::vector<VkAttachmentReference> Attachments::getColorReferences() const
    {
        uint32_t colorCount = getColorAttachmentCount();

        std::vector<VkAttachmentReference> colorReferences;

        for (uint32_t i = 0; i < colorCount; ++i)
        {
            colorReferences.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        }

        return colorReferences;
    }

    VkAttachmentReference Attachments::getDepthReference() const
    {
        uint32_t index = getColorAttachmentCount();

        return { index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    }

    std::vector<VkImageView> Attachments::getAttachementViews() const
    {
        uint32_t colorCount = getColorAttachmentCount();

        std::vector<VkImageView> attachmentViews(colorCount);

        for (uint32_t i = 0; i < colorCount; ++i)
        {
            attachmentViews[i] = colorAttachments_[i].texture->imageView();
        }

        if (depthAttachment_.texture)
        {
            attachmentViews.push_back(depthAttachment_.texture->imageView());
        }

        return attachmentViews;
    }

    VkExtent2D Attachments::getExtent() const
    {
        VkExtent2D extent = {};

        if (colorAttachments_.size() > 0)
        {
            extent.width = colorAttachments_[0].texture->width();
            extent.height = colorAttachments_[0].texture->height();
        }
        else if (depthAttachment_.texture)
        {
            extent.width = depthAttachment_.texture->width();
            extent.height = depthAttachment_.texture->height();
        }

        return extent;
    }

    void Attachments::setFinalLayouts(std::span<const VkAttachmentDescription> attachmentDescs)
    {
        uint32_t colorCount = getColorAttachmentCount();
        uint32_t depthIndex = colorCount; // the last index is depth

        for (uint32_t i = 0; i < attachmentDescs.size(); ++i)
        {
            if (i < colorCount)
            {
                colorAttachments_[i].finalLayout = attachmentDescs[i].finalLayout;
            }
            else if (i == depthIndex)
            {
                depthAttachment_.finalLayout = attachmentDescs[i].finalLayout;
            }
        }
    }


    // Render pass 
    RenderPass::~RenderPass()
    {
        if (renderPass_)
        {
            vkDestroyRenderPass(device_, renderPass_, nullptr);
        }
    }


    // Frame buffer
    Framebuffer::~Framebuffer()
    {
        if (framebuffer_)
        {
            vkDestroyFramebuffer(device_, framebuffer_, nullptr);
        }
    }

    Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    {
		device_ = std::exchange(other.device_, VK_NULL_HANDLE);
		framebuffer_ = std::exchange(other.framebuffer_, VK_NULL_HANDLE);
    }

    Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
	{
		if (this != &other) 
		{
			if (framebuffer_)
            {
                vkDestroyFramebuffer(device_, framebuffer_, nullptr);
            }

			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
		    framebuffer_ = std::exchange(other.framebuffer_, VK_NULL_HANDLE);
		}

		return *this;
	}
}