#pragma once

#include <vulkan/vulkan.h>

namespace vks
{
	class ShaderModule
	{
	public:
		ShaderModule() = default;
		~ShaderModule() 
		{ 
			if (shaderModule_) 
			{
				vkDestroyShaderModule(device_, shaderModule_, nullptr);
			}
		}

		ShaderModule(const ShaderModule&) = delete;
		ShaderModule& operator=(const ShaderModule&) = delete;

		VkShaderModule handle() const { return shaderModule_; }

	private:
		VkDevice device_ = VK_NULL_HANDLE;
		VkShaderModule shaderModule_;

		friend class AssetLoader;
	};

	inline VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(
		const ShaderModule& shaderModule,
		VkShaderStageFlagBits stage,
		const char* entryPoint = "main")
	{
		VkPipelineShaderStageCreateInfo shaderStage = {};
		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = stage;
		shaderStage.module = shaderModule.handle();
		shaderStage.pName = entryPoint;

		return shaderStage;
	}
}