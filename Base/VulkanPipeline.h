#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace vks
{
    class PipelineLayoutManager
    {
    public:
        explicit PipelineLayoutManager(VkDevice device): device_(device) {};
        ~PipelineLayoutManager();
        
        PipelineLayoutManager(const PipelineLayoutManager&) = delete;
        PipelineLayoutManager& operator=(const PipelineLayoutManager&) = delete;  

        VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo& pipelineLayoutInfo);

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        
        struct LayoutKey
        {   
            std::vector<VkDescriptorSetLayout> setLayouts;
            std::vector<VkPushConstantRange>   pushConstRanges;

            bool operator==(const LayoutKey& other) const;
        }; 

        struct LayoutKeyHash
        {
            size_t operator()(const LayoutKey& key) const noexcept;
        };

        using PipelineLayoutMap = std::unordered_map<LayoutKey, VkPipelineLayout, LayoutKeyHash>;
        PipelineLayoutMap pipelineLayoutMap_;
    };

    class VulkanContext;

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline();

        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        Pipeline(Pipeline&& other) noexcept;
        Pipeline& operator=(Pipeline&& other) noexcept;

        VkPipeline handle() const { return pipeline_; }
        VkPipelineLayout layout() const { return layout_; }
        VkPipelineBindPoint bindPoint() const { return bindPoint_; }
    
    private:
        VkDevice device_ = VK_NULL_HANDLE;

        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;

        friend class VulkanContext;
    };
}