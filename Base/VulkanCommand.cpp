#include "VulkanCommand.h"
#include "VulkanglTFModel.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanContext.h"
#include "VulkanTools.h"

namespace vks 
{
    /**
    *   Command list
    */
    void CommandList::beginCommandBuffer()
    {
        VkCommandBufferBeginInfo cmdBufInfo = initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer_, &cmdBufInfo));
    }

    void CommandList::beginRenderPass(
        const RenderPass& renderPass,
        const Framebuffer& framebuffer,
        std::span<VkClearValue> clearValues, 
        uint32_t renderAreaWidth,
        uint32_t renderAreaHeight)
    {
        assert(queueType_ == QueueType::Graphics);

        VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
        renderPassBeginInfo.renderPass = renderPass.handle();
        renderPassBeginInfo.framebuffer = framebuffer.handle();
        renderPassBeginInfo.renderArea.extent.width = renderAreaWidth;
        renderPassBeginInfo.renderArea.extent.height = renderAreaHeight;
        renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer_, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void CommandList::setViewport(float width, float height)
    {
        assert(queueType_ == QueueType::Graphics);

        VkViewport viewport = initializers::viewport(width, height, 0.f, 1.f);
        vkCmdSetViewport(commandBuffer_, 0, 1, &viewport);
    }

    void CommandList::setScissor(int32_t width, int32_t height)
    {
        assert(queueType_ == QueueType::Graphics);

        VkRect2D scissor = initializers::rect2D(width, height, 0, 0);
        vkCmdSetScissor(commandBuffer_, 0, 1, &scissor);
    }

    void CommandList::bindDescriptorSets(const Pipeline& pipeline, const DescriptorSet& descriptorSet)
    {
        VkDescriptorSet descriptor = descriptorSet.handle();

        vkCmdBindDescriptorSets(
            commandBuffer_, 
            pipeline.bindPoint(), pipeline.layout(), 
            0, 1, &descriptor, 
            0, nullptr);
    }

    void CommandList::bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets)
    {
        vkCmdBindVertexBuffers(commandBuffer_, firstBinding,bindingCount, pBuffers, pOffsets);
    }

    void CommandList::bindPipeline(const Pipeline& pipeline)
    {
        vkCmdBindPipeline(commandBuffer_, pipeline.bindPoint(), pipeline.handle());
    }

    void CommandList::pushConstants(const Pipeline& pipeline, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
    {
        vkCmdPushConstants(commandBuffer_, pipeline.layout(), stageFlags, offset, size, pValues);
    }

    void CommandList::draw(vkglTF::Model& model)
    {
        assert(queueType_ == QueueType::Graphics);

        model.draw(commandBuffer_);
    }

    void CommandList::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        assert(queueType_ == QueueType::Graphics);

        vkCmdDraw(commandBuffer_, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandList::endRenderPass()
    {
        assert(queueType_ == QueueType::Graphics);
        
        vkCmdEndRenderPass(commandBuffer_);
    }

    void CommandList::pipelineBarrier2(const PipelineBarrier& pipelineBarrier)
    {
        if (pipelineBarrier.empty()) return;

        std::span<const VkMemoryBarrier2> memoryBarriers = pipelineBarrier.memoryBarriers();
        std::span<const VkBufferMemoryBarrier2> bufferBarriers = pipelineBarrier.bufferBarriers();
        std::span<const VkImageMemoryBarrier2> imageBarriers = pipelineBarrier.imageMemoryBarriers();

        VkDependencyInfo dependencyInfo = {};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.memoryBarrierCount = static_cast<uint32_t>(memoryBarriers.size());
        dependencyInfo.pMemoryBarriers = memoryBarriers.data();
        dependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size());
        dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
        dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
        dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

        vkCmdPipelineBarrier2(commandBuffer_, &dependencyInfo);
    }

    void CommandList::endCommandBuffer()
    {
        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer_));
    }

    void CommandList::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        vkCmdDispatch(commandBuffer_, groupCountX, groupCountY, groupCountZ);
    }

    void CommandList::fillBuffer(const Buffer& buffer, uint32_t data, VkDeviceSize dstOffset, VkDeviceSize size)
    {
        assert(buffer.usageFlags() & VK_BUFFER_USAGE_TRANSFER_DST_BIT);

        if (size == VK_WHOLE_SIZE) 
        {
            size = buffer.size();
        }
        
        assert(dstOffset + size <= buffer.size());

        vkCmdFillBuffer(commandBuffer_, buffer.handle(), dstOffset, size, data);
    }

    void CommandList::copyImage(const Texture& srcImage, const Texture& dstImage)
    {
        assert(srcImage.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        assert(dstImage.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        uint32_t width  = std::min(srcImage.width(),  dstImage.width());
        uint32_t height = std::min(srcImage.height(), dstImage.height());

        VkImageCopy region = {};
        region.srcSubresource.aspectMask = srcImage.aspectMask();
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource = region.srcSubresource;
        region.extent = { width, height, 1 };

        vkCmdCopyImage(
            commandBuffer_, 
            srcImage.image(), 
            srcImage.getCurrentLayout(), 
            dstImage.image(), 
            dstImage.getCurrentLayout(), 
            1, 
            &region);
    }

    void CommandList::copyBufferToImage(const Buffer& srcBuffer, const Texture& dstImage, std::span<const VkBufferImageCopy> regions)
    {
        assert(dstImage.getCurrentLayout() == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdCopyBufferToImage(
            commandBuffer_,
            srcBuffer.handle(),
            dstImage.image(),
            dstImage.getCurrentLayout(),
            static_cast<uint32_t>(regions.size()),
            regions.data());
    }
    
    /**
    *   Command context
    */
    void CommandContext::useTexture(
        Texture& texture, 
        TextureUsage dstUsage,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex)
    {
        bool hasTransition = barriers_.addImageMemoryBarrier(texture, dstUsage, srcQueueFamilyIndex, dstQueueFamilyIndex);
        if (hasTransition)
        {
            TextureTransition transition = { &texture, dstUsage };
            textureTransitions_.push_back(transition);
        }
    }

    void CommandContext::useBuffer(
        Buffer& buffer, 
        BufferUsage dstUsage,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex)
    {
        bool hasTransition = barriers_.addBufferMemoryBarrier(buffer, dstUsage, srcQueueFamilyIndex, dstQueueFamilyIndex);
        if (hasTransition)
        {
            BufferTransition transition = { &buffer, dstUsage };
            bufferTransitions_.push_back(transition);
        }
    }

    void CommandContext::acquireBuffer(Buffer& buffer, BufferUsage dstUsage, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex)
    {
        barriers_.acquireBufferMemoryBarrier(buffer, dstUsage, srcQueueFamilyIndex, dstQueueFamilyIndex);

        BufferTransition transition = { &buffer, dstUsage };
        bufferTransitions_.push_back(transition);
    }

    void CommandContext::releaseBuffer(Buffer& buffer, BufferUsage srcUsage, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex)
    {
        barriers_.releaseBufferMemoryBarrier(buffer, srcUsage, srcQueueFamilyIndex, dstQueueFamilyIndex);
    }

    void CommandContext::flushBarriers(CommandList& commandList)
    {
        commandList.pipelineBarrier2(barriers_);
        barriers_.clear();
    }

    void CommandContext::commitTransitions()
    {
        for (const auto& bufferTransition : bufferTransitions_)
        {
            if  (bufferTransition.buffer)
            {
                bufferTransition.buffer->setCurrentUsage(bufferTransition.dstUsage);
            }
        }
        bufferTransitions_.clear();

        for (const auto& textureTransition : textureTransitions_)
        {
            if (textureTransition.texture)
            {
                textureTransition.texture->setCurrentUsage(textureTransition.dstUsage);
                textureTransition.texture->setCurrentLayout(vks::getDefaultLayout(textureTransition.dstUsage));
            }
        }
        textureTransitions_.clear();
    }

    void CommandContext::commitAttachmentLayouts(const Attachments& attachments)
    {
        for (const auto& attachment : attachments.colorAttachments())
        {
            if (attachment.texture)
            {
                attachment.texture->setCurrentLayout(attachment.finalLayout);
            }
        }

        const auto& depthAttachment = attachments.depthAttachment();

        if (depthAttachment.texture)
        {
            depthAttachment.texture->setCurrentLayout(depthAttachment.finalLayout);
        }
    }
}