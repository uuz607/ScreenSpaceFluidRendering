#include "VulkanBarriers.h"
#include "VulkanTools.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"

namespace vks 
{
    struct StageAccessInfo
    {
        VkPipelineStageFlags2 stageMask;
        VkAccessFlags2        accessMask;
    };

    inline StageAccessInfo getStageAccess(TextureUsage usage)
	{
		switch (usage) 
		{
        case TextureUsage::TransferSrc:
            return {
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT
            };
        
        case TextureUsage::TransferDst:
            return {
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT
            };

		case TextureUsage::ColorAttachmentWrite:
			return { 
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT 
            };

		case TextureUsage::DepthAttachmentWrite:
			return { 
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT 
            };

        case TextureUsage::DepthAttachmentRead:
            return {
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            };

        case TextureUsage::ShaderSampledRead:
            return {
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            };
        
        case TextureUsage::ShaderStorageWrite:
            return {
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_WRITE_BIT
            };
        
        case TextureUsage::Present:
            return {
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
            };
        
        case TextureUsage::ComputeShaderRead:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            };
        
        case TextureUsage::ComputeShaderWrite:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_WRITE_BIT
            };
        
        default:
            return { VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
		}
    }

    inline StageAccessInfo getStageAccess(BufferUsage usage)
    {
        switch (usage) 
		{
		case BufferUsage::VertexRead:
			return { 
                VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, 
                VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT 
            };

		case BufferUsage::IndexRead:
			return { 
                VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
                VK_ACCESS_2_INDEX_READ_BIT  
            };

        case BufferUsage::IndirectRead:
            return {
                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT
            };
    
        case BufferUsage::UniformRead:
            return {
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_UNIFORM_READ_BIT
            };

        case BufferUsage::ShaderRead:
            return {
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            };

        case BufferUsage::ShaderWrite:
            return {
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_WRITE_BIT
            };
        
        case BufferUsage::TransferSrc:
            return {
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT
            };
        
        case BufferUsage::TransferDst:
            return {
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT
            };
        
        case BufferUsage::ComputeUniformRead:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_UNIFORM_READ_BIT
            };

        case BufferUsage::ComputeShaderRead:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT
            };
        
        case BufferUsage::ComputeShaderWrite:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_WRITE_BIT
            };

        case BufferUsage::ComputeShaderReadWrite:
            return {
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT
            };

        default:
            return { VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE };
		}

    }

    inline bool isHostUsage(BufferUsage usage) 
    {
        return usage == BufferUsage::HostWrite;
    }

    inline bool isWriteUsage(TextureUsage usage)
    {
        if (usage == TextureUsage::TransferDst ||
            usage == TextureUsage::ColorAttachmentWrite ||
            usage == TextureUsage::DepthAttachmentWrite ||
            usage == TextureUsage::ShaderStorageWrite ||
            usage == TextureUsage::Present ||
            usage == TextureUsage::ComputeShaderWrite ||
            usage == TextureUsage::Undefined ) 
        {
            return true;
        }
        else 
        {
            return false;
        }
    }

    inline bool isWriteUsage(BufferUsage usage)
    {
        if (usage == BufferUsage::ShaderWrite || 
            usage == BufferUsage::ComputeShaderWrite ||
            usage == BufferUsage::ComputeShaderReadWrite ||
            usage == BufferUsage::TransferDst || 
            usage == BufferUsage::Undefined) 
        {
            return true;
        }
        else 
        {
            return false;
        }
    }

    inline bool needBarrier(const Texture& texture, TextureUsage dstUsage)
    {
        TextureUsage  srcUsage  = texture.getCurrentUsage();  
        VkImageLayout oldLayout = texture.getCurrentLayout();
        VkImageLayout newLayout = vks::getDefaultLayout(dstUsage);

        if (oldLayout != newLayout || isWriteUsage(srcUsage)) 
        {
            return true;
        }
        else 
        {
            return false;
        }
    }

    inline bool needBarrier(const Buffer& buffer)
    {
        BufferUsage srcUsage = buffer.getCurrentUsage();
        if (isWriteUsage(srcUsage)) 
        {
            return true;
        }
        else 
        {
            return false;
        }
    }

    bool PipelineBarrier::addImageMemoryBarrier(
        const Texture& texture,
        TextureUsage dstUsage,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex)
    {
        if (!needBarrier(texture, dstUsage))
        {
            return false;
        }

        TextureUsage  srcUsage  = texture.getCurrentUsage();  
        VkImageLayout oldLayout = texture.getCurrentLayout();
        VkImageLayout newLayout = vks::getDefaultLayout(dstUsage);

        StageAccessInfo srcInfo = vks::getStageAccess(srcUsage);
        StageAccessInfo dstInfo = vks::getStageAccess(dstUsage);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = texture.aspectMask();
        subresourceRange.layerCount = texture.layerCount();
        subresourceRange.levelCount = texture.levelCount();

        VkImageMemoryBarrier2 imageBarrier = {};
        imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        imageBarrier.srcStageMask = srcInfo.stageMask;
        imageBarrier.srcAccessMask = srcInfo.accessMask;
        imageBarrier.dstStageMask = dstInfo.stageMask;
        imageBarrier.dstAccessMask = dstInfo.accessMask;
        imageBarrier.oldLayout = oldLayout;
        imageBarrier.newLayout = newLayout;
        imageBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
        imageBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
        imageBarrier.image = texture.image();
        imageBarrier.subresourceRange = subresourceRange;

        imageBarriers_.push_back(imageBarrier);

        return true;
    }

    bool PipelineBarrier::addBufferMemoryBarrier(
        const Buffer& buffer, 
        BufferUsage dstUsage,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex)
    {
        if (!needBarrier(buffer)) 
        {
            return false;
        }

        BufferUsage srcUsage = buffer.getCurrentUsage();
        StageAccessInfo srcInfo = vks::getStageAccess(srcUsage);
        StageAccessInfo dstInfo = vks::getStageAccess(dstUsage);

        VkBufferMemoryBarrier2 bufferBarrier = {};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        bufferBarrier.srcStageMask = srcInfo.stageMask;
        bufferBarrier.srcAccessMask = srcInfo.accessMask;
        bufferBarrier.dstStageMask = dstInfo.stageMask;
        bufferBarrier.dstAccessMask = dstInfo.accessMask;
        bufferBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
        bufferBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
        bufferBarrier.buffer = buffer.handle();
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;

        bufferBarriers_.push_back(bufferBarrier);

        return true;
    }

    void PipelineBarrier::acquireBufferMemoryBarrier(
        const Buffer& buffer, 
        BufferUsage dstUsage, 
        uint32_t srcQueueFamilyIndex, 
        uint32_t dstQueueFamilyIndex)
    {
        StageAccessInfo dstInfo = vks::getStageAccess(dstUsage);

        VkBufferMemoryBarrier2 bufferBarrier = {};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        bufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        bufferBarrier.srcAccessMask = VK_ACCESS_2_NONE;
        bufferBarrier.dstStageMask = dstInfo.stageMask;
        bufferBarrier.dstAccessMask = dstInfo.accessMask;
        bufferBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
        bufferBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
        bufferBarrier.buffer = buffer.handle();
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;

        bufferBarriers_.push_back(bufferBarrier);
    }

    void PipelineBarrier::releaseBufferMemoryBarrier(
        const Buffer& buffer, 
        BufferUsage srcUsage, 
        uint32_t srcQueueFamilyIndex, 
        uint32_t dstQueueFamilyIndex)
    {
        StageAccessInfo srcInfo = vks::getStageAccess(srcUsage);

        VkBufferMemoryBarrier2 bufferBarrier = {};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        bufferBarrier.srcStageMask = srcInfo.stageMask;
        bufferBarrier.srcAccessMask = srcInfo.accessMask;
        bufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        bufferBarrier.dstAccessMask = VK_ACCESS_2_NONE;
        bufferBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
        bufferBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
        bufferBarrier.buffer = buffer.handle();
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;

        bufferBarriers_.push_back(bufferBarrier);
    }

    void PipelineBarrier::clear()
    {
        memoryBarriers_.clear();
        bufferBarriers_.clear();
        imageBarriers_.clear();
    }

    bool PipelineBarrier::empty() const
    {
        return memoryBarriers_.empty() && bufferBarriers_.empty() && imageBarriers_.empty();
    }
}