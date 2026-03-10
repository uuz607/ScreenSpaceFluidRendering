#pragma once

#include "VulkanBarriers.h"
#include <vector>

namespace vkglTF 
{
    class Model;
}

namespace vks 
{
    
    class Attachments;
    class RenderPass;
    class Framebuffer;
    class PipelineBarrier;

    class VulkanContext;
    
    class Pipeline;
    class DescriptorSet;

    enum class QueueType : uint32_t;

    /**
    *   Command list
    */
    class CommandList
    {
    public:
        CommandList() = default;
        ~CommandList() = default;

        VkCommandBuffer handle() const { return commandBuffer_; };
        QueueType queueType() const { return queueType_; }

        void beginCommandBuffer();

        void beginRenderPass(
            const RenderPass& renderPass,
            const Framebuffer& framebuffer,
            std::span<VkClearValue> clearValues, 
            uint32_t renderAreaWidth,
            uint32_t renderAreaHeight);

        void setViewport(float width, float height);

        void setScissor(int32_t width, int32_t height);

        void bindDescriptorSets(const Pipeline& pipeline, const DescriptorSet& descriptorSet);

        void bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets);

        void bindPipeline(const Pipeline& pipeline);

        void pushConstants(
            const Pipeline& pipeline, 
            VkShaderStageFlags stageFlags, 
            uint32_t offset, 
            uint32_t size, 
            const void* pValues);
        
        void draw(vkglTF::Model& model);
        void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        
        void endRenderPass();
       
        void pipelineBarrier2(const PipelineBarrier& pipelineBarrier);
        
        void endCommandBuffer();
        
        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

        void fillBuffer(const Buffer& buffer, uint32_t data, VkDeviceSize dstOffset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        void copyImage(const Texture& srcImage, const Texture& dstImage);
        void copyBufferToImage(const Buffer& srcBuffer, const Texture& dstImage, std::span<const VkBufferImageCopy> regions);

    private:
        VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
        QueueType queueType_;

        friend class VulkanContext;
    };


    /**
    *   Command context
    */
    class CommandContext
    {
    public:
        CommandContext() = default;
        ~CommandContext() = default;
        
        CommandContext(const CommandContext&) = delete;
		CommandContext& operator=(const CommandContext&) = delete;

        void useTexture(
            Texture& texture, 
            TextureUsage dstUsage,
            uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

        void useBuffer(
            Buffer& buffer, 
            BufferUsage dstUsage,
            uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

        void acquireBuffer(Buffer& buffer, BufferUsage dstUsage, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex);
        void releaseBuffer(Buffer& buffer, BufferUsage srcUsage, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex);

        void flushBarriers(CommandList& commandList);
        void commitTransitions();
        void commitAttachmentLayouts(const Attachments& attachments);
    
    private:
        struct BufferTransition
        {
            Buffer* buffer;
            BufferUsage dstUsage;
        };

        struct TextureTransition
        {
            Texture* texture;
            TextureUsage dstUsage;  
        };

    private:
        std::vector<BufferTransition> bufferTransitions_;
        std::vector<TextureTransition> textureTransitions_;

        PipelineBarrier barriers_;
    };

}