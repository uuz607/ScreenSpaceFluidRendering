#pragma once

#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <span>


namespace vks 
{
    class SamplerManager;
    class DescriptorSetLayoutManager;
    class PipelineLayoutManager;
    class DescriptorSet;
    class CommandList;
    class Pipeline;
    class ShaderModule;
    class RenderPass;
    class Framebuffer;
    class Sampler;
    class Attachments;

    struct TextureCreateInfo;
    enum class TextureType : uint8_t;
    class Texture;

    enum class QueueType : uint32_t
    {
        Graphics,
        Compute,
        Transfer
    };

    class VulkanContext
    {
    public:
        VulkanContext() = default;
        ~VulkanContext();

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        void createContext(VulkanDevice* vulkanDevice);

        void createSampler(Sampler& sampler, const VkSamplerCreateInfo& samplerInfo);

        void createDescriptorSet(DescriptorSet& outDescriptorSet, std::span<VkDescriptorSetLayoutBinding> layoutBindings);

        void createBuffer(
            Buffer& outBuffer, 
            VkBufferUsageFlags usageFlags, 
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize size, 
            const void* data = nullptr);

        template<typename T>
        void createUniformBuffer(UniformBuffer<T>& outBuffer, const T* data = nullptr);

        template<typename T>
        void createDeviceBuffer(DeviceBuffer<T>& outBuffer, VkBufferUsageFlags usageFlags, uint32_t count, const T* data = nullptr);

        void createTexture(Texture& outTexture, const TextureCreateInfo& textureInfo);

        void createRenderPass(RenderPass& outRenderPass, const VkRenderPassCreateInfo& renderPassInfo);
        void createRenderPass(
            RenderPass& outRenderPass, 
            const Attachments& attachments,
            std::span<const VkAttachmentDescription> attachmentDescs);

        void createFramebuffer(Framebuffer& outFrameBuffer, const VkFramebufferCreateInfo& framebufferInfo);
        void createFramebuffer(Framebuffer& outFrameBuffer, const RenderPass& renderPass, const Attachments& attachments);

        void createGraphicsPipeline(Pipeline& outPipeline, const VkGraphicsPipelineCreateInfo& pipelineCreateInfo);
        void createComputePipeline(Pipeline& outPipeline, const VkComputePipelineCreateInfo& pipelineCreateInfo);

        void createCommandList(
            CommandList& outCommandList, 
            bool begin = false,
            QueueType queueType = QueueType::Graphics, 
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        void flushCommandList(CommandList& commandList, bool free = true);

        
        VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& pipelineLayoutInfo);
        
        VulkanDevice* vulkanDevice() const { return vulkanDevice_; }
        VkDevice device() const { return logicalDevice_; }
        VkQueue transferQueue() const { return graphicsQueue_; }
        VkQueue graphicsQueue() const { return graphicsQueue_; }
        VkQueue computeQueue() const { return computeQueue_; }
        
        VkFormat depthFormat() const { return depthFormat_; }

    private:
        // Handle to the device graphics queue that command buffers are submitted to
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkQueue computeQueue_ = VK_NULL_HANDLE;

        // Command buffer pool
        VkCommandPool graphicsCommandPool_ = VK_NULL_HANDLE;
        VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;

        // Descriptor set pool
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
        // Pipeline cache object
        VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

        // Depth buffer format (selected during Vulkan initialization)
	    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
        
        VkDevice logicalDevice_ = VK_NULL_HANDLE;

        VulkanDevice* vulkanDevice_ = nullptr;
        SamplerManager* samplerManager_ = nullptr;
        DescriptorSetLayoutManager* descriptorSetLayoutManager_ = nullptr;
        PipelineLayoutManager* pipelineLayoutManager_ = nullptr;

        bool requireStencil_ = false;
    };

    template<typename T>
    void VulkanContext::createUniformBuffer(UniformBuffer<T>& outBuffer, const T* data)
    {
        createBuffer(
            outBuffer, 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            sizeof(T), data);

        outBuffer.currentBufferUsage_ = BufferUsage::HostWrite;
    }

    template<typename T>
    void VulkanContext::createDeviceBuffer(DeviceBuffer<T>& outBuffer, VkBufferUsageFlags usageFlags, uint32_t count, const T* data)
    {
        outBuffer.count_ = count;
        VkDeviceSize size = sizeof(T) * count;
        
        if(data != nullptr)
        {
            usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        createBuffer(outBuffer, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, size);
		outBuffer.currentBufferUsage_ = BufferUsage::Undefined;

        if(data != nullptr)
        {
            vks::Buffer stagingBuffer;
            createBuffer(
                stagingBuffer, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                size, data); 

            VkCommandBuffer copyCmd = vulkanDevice_->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            VkBufferCopy copyRegion = {};
			copyRegion.size = size;
			vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer_, outBuffer.buffer_, 1, &copyRegion);
            vulkanDevice_->flushCommandBuffer(copyCmd, graphicsQueue_);

			outBuffer.currentBufferUsage_ = BufferUsage::TransferDst;
        }
    }
}