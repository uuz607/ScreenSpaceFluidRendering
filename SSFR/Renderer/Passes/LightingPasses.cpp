#include "LightingPasses.h"
#include "VulkanSampler.h"
#include "VulkanContext.h"
#include "AssetLoader.h"
#include "VulkanShader.hpp"
#include "VulkanCommand.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"

#include <array>

namespace ssfr
{
    /**
    *   FluidLightingPass
    */
    void FluidLightingPass::create(vks::VulkanContext* context, const FluidLightingPassAttachments& attachments)
    {
        context_ = context;

        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void FluidLightingPass::setResources(const FluidLightingPassResources& resources)
    {
        resources_ = resources;

        // Write descriptor sets
        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorBufferInfo paramsDescInfo = resources_.fluid.params->descriptorInfo();
        VkDescriptorBufferInfo posDescInfo = resources_.fluid.position->descriptorInfo();

        VkDescriptorImageInfo viewNormalDescInfo = vks::descriptorImageInfo(*resources_.fluid.viewNormal, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo thicknessDescInfo = vks::descriptorImageInfo(*resources_.fluid.thickness, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo depthDescInfo = vks::descriptorImageInfo(*resources_.fluid.smoothedDepth, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo sceneColorDescInfo = vks::descriptorImageInfo(*resources_.scene.color, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo skyboxDescInfo = vks::descriptorImageInfo(*resources_.scene.skybox, vks::TextureUsage::ShaderSampledRead, &skyboxSampler_);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &paramsDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &posDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &viewNormalDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &thicknessDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &depthDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, &sceneColorDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7, &skyboxDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FluidLightingPass::record(vks::CommandList& commandList)
    {
        vks::CommandContext commandContext;

        for(const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        commandContext.useTexture(*attachments_.depthAttachment().texture, vks::TextureUsage::DepthAttachmentWrite);

        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::UniformRead);
        commandContext.useBuffer(*resources_.fluid.params, vks::BufferUsage::UniformRead);

        commandContext.useTexture(*resources_.scene.skybox, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.scene.color, vks::TextureUsage::ShaderSampledRead);

        commandContext.useBuffer(*resources_.fluid.position, vks::BufferUsage::ShaderRead);
        commandContext.useTexture(*resources_.fluid.viewNormal, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.fluid.thickness, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.fluid.smoothedDepth, vks::TextureUsage::ShaderSampledRead);

        // Acquire buffer
        uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
        uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
        if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
        {
            commandContext.acquireBuffer(*resources_.fluid.position, vks::BufferUsage::ShaderRead, srcQueueFamilyIndex, dstQueueFamilyIndex);
        }
        else 
        {
            commandContext.useBuffer(*resources_.fluid.position, vks::BufferUsage::ShaderRead);
        }

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_.getExtent();
        uint32_t width = extent.width;
        uint32_t height = extent.height;
        uint32_t particleCount = resources_.fluid.position->count();

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[Color].color = DefaultClearColor;
        clearValues[Depth].depthStencil.depth = 1.f;

        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);      
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(6, particleCount, 0, 0);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);

        // Release barrier
        srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
        dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
        if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
        {
            commandContext.releaseBuffer(*resources_.fluid.position, vks::BufferUsage::ShaderRead, srcQueueFamilyIndex, dstQueueFamilyIndex);
        }
        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();
    }

    void FluidLightingPass::onWindowResize()
    {
        createFramebuffer();
    }

    void FluidLightingPass::setupRenderPass(const FluidLightingPassAttachments& attachments)
    {
        attachments_.addColorAttachment(attachments.color); // 0-color
        attachments_.addDepthAttachment(attachments.depth); // 1-depth

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Color] = vks::makeDefaultDescription(*attachments.color, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[Depth] = vks::makeDefaultDescription(*attachments.depth, vks::TextureUsage::DepthAttachmentWrite, vks::TextureUsage::DepthAttachmentWrite);
        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void FluidLightingPass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void FluidLightingPass::setupDescriptors()
    {
        // Create samplers
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        context_->createSampler(skyboxSampler_, samplerInfo);

        samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
        context_->createSampler(sampler_, samplerInfo);

        // Create descriptor sets
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 7)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FluidLightingPass::preparePipeline()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo();
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
        
        // Empty vertex input state, vertices are generated by the vertex shader
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState = &emptyInputState;

        // Create pipeline layout
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        pipelineCreateInfo.layout = context_->createPipelineLayout(pipelineLayoutInfo);

        // Set color blend attachments
        uint32_t colorCount = attachments_.getColorAttachmentCount();
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(colorCount, blendAttachmentState); 
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(colorCount, blendAttachmentStates.data());
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

        // Set shader stages
        std::string shaderPath = getShaderBasePath();

        vks::ShaderModule shaderModules[2];
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/Particle.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Lighting/Fluid.frag.spv");

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            vks::pipelineShaderStageCreateInfo(shaderModules[0], VK_SHADER_STAGE_VERTEX_BIT),
            vks::pipelineShaderStageCreateInfo(shaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT)
        };
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages = shaderStages.data();

        // Set render pass
        pipelineCreateInfo.renderPass = renderPass_.handle();

        context_->createGraphicsPipeline(pipeline_, pipelineCreateInfo);
    }


    /**
    *   Resolve pass
    */
    void ResolvePass::create(vks::VulkanContext* context, std::vector<vks::Texture*> swapChainImages)
    {
        context_ = context;

        setupRenderPass(swapChainImages);
        createFramebuffer();
        setupDescriptors(swapChainImages);
        preparePipeline();
    }

    void ResolvePass::setResources(const ResolvePassResources& resources)
    {
        resources_ = resources;

        // Write descriptor sets        
        VkDescriptorImageInfo sceneColorTexInfo = vks::descriptorImageInfo(*resources_.scene.color, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo sceneDepthTexInfo = vks::descriptorImageInfo(*resources_.scene.depth, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo fluidColorTexInfo = vks::descriptorImageInfo(*resources_.fluid.color, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo fluidDepthTexInfo = vks::descriptorImageInfo(*resources_.fluid.depth, vks::TextureUsage::ShaderSampledRead, &sampler_);
        VkDescriptorImageInfo fluidValidTexInfo = vks::descriptorImageInfo(*resources_.fluid.valid, vks::TextureUsage::ShaderSampledRead);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &sceneColorTexInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &sceneDepthTexInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &fluidColorTexInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &fluidDepthTexInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4, &fluidValidTexInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void ResolvePass::record(vks::CommandList& commandList, uint32_t index)
    {
        vks::CommandContext commandContext;

        for(const auto& color : attachments_[index].colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }

        commandContext.useTexture(*resources_.scene.color, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.scene.depth, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.fluid.color, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.fluid.depth, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.fluid.valid, vks::TextureUsage::ShaderSampledRead);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_[index].getExtent(); 
        uint32_t width = extent.width;
        uint32_t height = extent.height;

        VkClearValue clearValue[1] = {};
        clearValue[0].color = DefaultClearColor;

        commandList.beginRenderPass(renderPass_, framebuffers_[index], clearValue, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);            
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(3, 1, 0, 0);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_[index]);
    }

    void ResolvePass::onWindowResize()
    {
        for (size_t i = 0; i < attachments_.size(); ++i)
        {
            context_->createFramebuffer(framebuffers_[i], renderPass_, attachments_[i]);
        }
    }

    void ResolvePass::setupRenderPass(std::vector<vks::Texture*> swapChainImages)
    {
        size_t imageCount = swapChainImages.size();
        attachments_.resize(imageCount);

        for (size_t i = 0; i < imageCount; ++i) 
        {
            attachments_[i].addColorAttachment(swapChainImages[i]); // 0-color
        }

        // The framebuffer for the swapchain images shares the same render pass.
        VkAttachmentDescription attachmentDescs[1] = { vks::makeDefaultDescription(*swapChainImages[0], vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::Present) };
        context_->createRenderPass(renderPass_, attachments_[0], attachmentDescs);

        for (auto& attachment : attachments_) 
        {
            attachment.setFinalLayouts(attachmentDescs);
        }
    }

    void ResolvePass::createFramebuffer()
    {
        size_t attachmentCount = attachments_.size();
        framebuffers_.resize(attachmentCount);

        for (size_t i = 0; i < attachmentCount; ++i)
        {
            context_->createFramebuffer(framebuffers_[i], renderPass_, attachments_[i]);
        }
    }

    void ResolvePass::setupDescriptors(std::vector<vks::Texture*> swapChainImages)
    {
        // Create samplers
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
        context_->createSampler(sampler_, samplerInfo);

        // Create descriptor sets
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        };

        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void ResolvePass::preparePipeline()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo();
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
        
        // Empty vertex input state, vertices are generated by the vertex shader
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState = &emptyInputState;

        // Create pipeline layout
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        pipelineCreateInfo.layout = context_->createPipelineLayout(pipelineLayoutInfo);

        // Set color blend attachments
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(1, blendAttachmentState); 
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, blendAttachmentStates.data());
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

        // Set shader stages
        std::string shaderPath = getShaderBasePath();

        vks::ShaderModule shaderModules[2];
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/FullScreen.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Lighting/Resolve.frag.spv");

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            vks::pipelineShaderStageCreateInfo(shaderModules[0], VK_SHADER_STAGE_VERTEX_BIT),
            vks::pipelineShaderStageCreateInfo(shaderModules[1], VK_SHADER_STAGE_FRAGMENT_BIT)
        };
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages = shaderStages.data();

        // Set render pass
        pipelineCreateInfo.renderPass = renderPass_.handle();

        context_->createGraphicsPipeline(pipeline_, pipelineCreateInfo);
    }
}