#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <span>

namespace vks
{
    class DescriptorSetLayoutManager
    {
    public:
        explicit DescriptorSetLayoutManager(VkDevice device): device_(device) {};
        ~DescriptorSetLayoutManager();
        
        DescriptorSetLayoutManager(const DescriptorSetLayoutManager&) = delete;
        DescriptorSetLayoutManager& operator=(const DescriptorSetLayoutManager&) = delete;  

        VkDescriptorSetLayout createDescriptorSetLayout(std::span<const VkDescriptorSetLayoutBinding> layoutBindings);

    private:
        VkDevice device_ = VK_NULL_HANDLE;

        struct LayoutKey
        {
            std::vector<VkDescriptorSetLayoutBinding> layoutInfos;

            bool operator==(const LayoutKey& key) const;
        };

        struct LayoutKeyHash
        {
            size_t operator()(const LayoutKey& key) const noexcept;
        };

        using DescriptorSetLayoutMap = std::unordered_map<LayoutKey, VkDescriptorSetLayout, LayoutKeyHash>;
        DescriptorSetLayoutMap descriptorSetLayoutMap_;
    };

    
    class VulkanContext;

    class DescriptorSet
    {
    public:
        DescriptorSet() = default;
        ~DescriptorSet();

        DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

        DescriptorSet(DescriptorSet&& other) noexcept;
        DescriptorSet& operator=(DescriptorSet&& other) noexcept;

        VkDescriptorSet handle() const { return descriptorSet_; }
        VkDescriptorSetLayout layout() const { return layout_; }

        void updateDescriptorSets(std::span<VkWriteDescriptorSet> writeDescriptorSets);

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

        friend class VulkanContext;
    };
}