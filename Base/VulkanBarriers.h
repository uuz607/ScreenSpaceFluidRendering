#pragma once

#include <vector>
#include <span>
#include <vulkan/vulkan.h>

namespace vks 
{
    class Texture;
    enum class TextureUsage : uint32_t;
    
    class Buffer;
    enum class BufferUsage : uint32_t;

    class PipelineBarrier
    {
    public:
        PipelineBarrier() = default;
        ~PipelineBarrier() = default;

        bool addImageMemoryBarrier(
            const Texture& texture, 
            TextureUsage dstUsage, 
            uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

        bool addBufferMemoryBarrier(
            const Buffer& buffer, 
            BufferUsage dstUsage,
            uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);
        
        void acquireBufferMemoryBarrier(
            const Buffer& buffer, 
            BufferUsage dstUsage, 
            uint32_t srcQueueFamilyIndex, 
            uint32_t dstQueueFamilyIndex);

        void releaseBufferMemoryBarrier(
            const Buffer& buffer, 
            BufferUsage srcUsage, 
            uint32_t srcQueueFamilyIndex, 
            uint32_t dstQueueFamilyIndex);

        void clear();
        bool empty() const;

        std::span<const VkMemoryBarrier2> memoryBarriers() const { return memoryBarriers_; }
        std::span<const VkBufferMemoryBarrier2> bufferBarriers() const { return bufferBarriers_; }
        std::span<const VkImageMemoryBarrier2> imageMemoryBarriers() const { return imageBarriers_; }
        
    private:
        std::vector<VkMemoryBarrier2> memoryBarriers_;
        std::vector<VkBufferMemoryBarrier2> bufferBarriers_;
        std::vector<VkImageMemoryBarrier2> imageBarriers_;
    };
}