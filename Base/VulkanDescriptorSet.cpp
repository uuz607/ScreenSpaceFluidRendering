#include "VulkanDescriptorSet.h"
#include "VulkanTools.h"
#include "VulkanContext.h"

namespace vks
{
    bool DescriptorSetLayoutManager::LayoutKey::operator==(const LayoutKey& key) const
    {   
        auto equal = [](const VkDescriptorSetLayoutBinding& a, const VkDescriptorSetLayoutBinding& b){
            return a.binding == b.binding && 
                   a.descriptorType == b.descriptorType && 
                   a.descriptorCount == b.descriptorCount && 
                   a.stageFlags == b.stageFlags;
        };
        
        return layoutInfos.size() == key.layoutInfos.size() &&
               std::equal(layoutInfos.begin(), layoutInfos.end(), key.layoutInfos.begin(), equal);
    }

    size_t DescriptorSetLayoutManager::LayoutKeyHash::operator()(const LayoutKey& key) const noexcept
    {
        size_t hash = 0;

        for(const auto& info : key.layoutInfos)
        {
            tools::hashCombine(hash, info.binding);
            tools::hashCombine(hash, info.descriptorType);
            tools::hashCombine(hash, info.descriptorCount);
            tools::hashCombine(hash, info.stageFlags);
        }

        return hash;
    }
    
    DescriptorSetLayoutManager::~DescriptorSetLayoutManager()
    {
        for (const auto& [key, value] : descriptorSetLayoutMap_)
            vkDestroyDescriptorSetLayout(device_, value, nullptr);
    }

    VkDescriptorSetLayout DescriptorSetLayoutManager::createDescriptorSetLayout(std::span<const VkDescriptorSetLayoutBinding> layoutBindings)
    {
        LayoutKey key;
        for (const auto& bindings : layoutBindings)
        {
            key.layoutInfos.push_back(bindings);
        }

        std::sort(key.layoutInfos.begin(), key.layoutInfos.end(), 
            [](const auto& a, const auto& b) { 
                return a.binding < b.binding; 
            }
        );

        auto it = descriptorSetLayoutMap_.find(key);
        if (it != descriptorSetLayoutMap_.end()) return it->second;

        VkDescriptorSetLayout layout;
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo = initializers::descriptorSetLayoutCreateInfo(layoutBindings.data(), (uint32_t)layoutBindings.size());

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device_, &layoutCreateInfo, nullptr, &layout));
        descriptorSetLayoutMap_.emplace(std::move(key), layout);

        return layout;
    }

    // Descriptor set
    DescriptorSet::~DescriptorSet()
    {
        if (descriptorSet_ && descriptorPool_) 
        {
            vkFreeDescriptorSets(device_, descriptorPool_, 1, &descriptorSet_);
        }
    }

    DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
    {
		device_ = std::exchange(other.device_, VK_NULL_HANDLE);
		descriptorSet_ = std::exchange(other.descriptorSet_, VK_NULL_HANDLE);
        layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
        descriptorPool_ = std::exchange(other.descriptorPool_, VK_NULL_HANDLE);
    }

    DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
    {
        if (this != &other) 
		{
			if (descriptorSet_ && descriptorPool_) 
            {
                vkFreeDescriptorSets(device_, descriptorPool_, 1, &descriptorSet_);
            }

			device_ = std::exchange(other.device_, VK_NULL_HANDLE);
            descriptorSet_ = std::exchange(other.descriptorSet_, VK_NULL_HANDLE);
            layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
            descriptorPool_ = std::exchange(other.descriptorPool_, VK_NULL_HANDLE);
		}

		return *this;
    }

    void DescriptorSet::updateDescriptorSets(std::span<VkWriteDescriptorSet> writeDescriptorSets)
    {
        for (auto& write : writeDescriptorSets)
        {
            write.dstSet = descriptorSet_;
        }

        vkUpdateDescriptorSets(
            device_, 
            static_cast<uint32_t>(writeDescriptorSets.size()), 
            writeDescriptorSets.data(), 
            0, 
            nullptr);
    }
}