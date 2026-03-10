#include "VulkanPipeline.h"
#include "VulkanTools.h"
#include "VulkanContext.h"

namespace vks
{
    // PipelineLayout
    bool PipelineLayoutManager::LayoutKey::operator==(const LayoutKey& other) const
    {
        return setLayouts == other.setLayouts && 
               pushConstRanges.size() == other.pushConstRanges.size() &&
               std::memcmp(pushConstRanges.data(), other.pushConstRanges.data(), pushConstRanges.size() * sizeof(VkPushConstantRange));
    }

    PipelineLayoutManager::~PipelineLayoutManager()
    {
        for (const auto& [key, value] : pipelineLayoutMap_)
        {
            vkDestroyPipelineLayout(device_, value, nullptr);
        }
    }

    size_t PipelineLayoutManager::LayoutKeyHash::operator()(const LayoutKey& key) const noexcept
    {
        size_t hash = 0;

        vks::tools::hashCombine(hash, key.setLayouts.size());
        for (const auto& layout : key.setLayouts)
        {
            vks::tools::hashCombine(hash, layout);
        }

        vks::tools::hashCombine(hash, key.pushConstRanges.size());
        for (const auto& pushConst : key.pushConstRanges)
        {
            vks::tools::hashCombine(hash, pushConst.stageFlags);
            vks::tools::hashCombine(hash, pushConst.offset);
            vks::tools::hashCombine(hash, pushConst.size);
        }

        return hash;
    }

    VkPipelineLayout PipelineLayoutManager::createPipelineLayout(const VkPipelineLayoutCreateInfo& pipelineLayoutInfo)
    {
        LayoutKey key;

        for (uint32_t i = 0 ; i < pipelineLayoutInfo.setLayoutCount; ++i)
        {
            key.setLayouts.push_back(pipelineLayoutInfo.pSetLayouts[i]);
        }

        for (uint32_t i = 0 ; i < pipelineLayoutInfo.pushConstantRangeCount; ++i)
        {
            key.pushConstRanges.push_back(pipelineLayoutInfo.pPushConstantRanges[i]);
        }

        auto it = pipelineLayoutMap_.find(key);
        if (it != pipelineLayoutMap_.end()) return it->second;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        VK_CHECK_RESULT(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout));
        pipelineLayoutMap_.emplace(std::move(key), pipelineLayout);

        return pipelineLayout;
    }

    // Pipeline
    Pipeline::~Pipeline()
    {
        if (pipeline_)
        {
           vkDestroyPipeline(device_, pipeline_, nullptr); 
        }
    }

    Pipeline::Pipeline(Pipeline&& other) noexcept
    {
        device_ = std::exchange(other.device_, VK_NULL_HANDLE);
        pipeline_ = std::exchange(other.pipeline_, VK_NULL_HANDLE);
        layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
        bindPoint_ = std::exchange(other.bindPoint_, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }

    Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
    {
        if (this != &other) 
		{
			if (pipeline_)
            {
                vkDestroyPipeline(device_, pipeline_, nullptr); 
            }

			device_ = std::exchange(other.device_, nullptr);
            pipeline_ = std::exchange(other.pipeline_, VK_NULL_HANDLE);
            layout_ = std::exchange(other.layout_, VK_NULL_HANDLE);
            bindPoint_ = std::exchange(other.bindPoint_, VK_PIPELINE_BIND_POINT_GRAPHICS);
		}

		return *this;
    }
}