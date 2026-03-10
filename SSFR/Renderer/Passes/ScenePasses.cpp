#include "ScenePasses.h"
#include "VulkanContext.h"
#include "VulkanInitializers.hpp"
#include "VulkanShader.hpp"
#include "AssetLoader.h"
#include "VulkanCommand.h"
#include "VulkanglTFModel.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"

namespace ssfr
{
    /**
    *   SkyboxPass
    */ 
    void SkyboxPass::create(vks::VulkanContext* context, const SkyboxPassAttachments& attachments)
    {   
        context_ = context;

        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void SkyboxPass::setResources(const SkyboxPassResources& resources)
    {
        resources_ = resources;

        //
        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorImageInfo skyboxDescInfo = vks::descriptorImageInfo(*resources_.skyboxTexture, vks::TextureUsage::ShaderSampledRead, &sampler_);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &skyboxDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void SkyboxPass::record(vks::CommandList& commandList)
    {
        // Transitions
        vks::CommandContext commandContext;

        // Render targets
        for(const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        commandContext.useTexture(*attachments_.depthAttachment().texture, vks::TextureUsage::DepthAttachmentWrite);
        
        // Shader inputs
        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::ShaderRead);
        commandContext.useTexture(*resources_.skyboxTexture, vks::TextureUsage::ShaderSampledRead);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        // Record
        VkExtent2D extent = attachments_.getExtent(); 
        uint32_t width = extent.width;
        uint32_t height = extent.height;

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[Color].color = DefaultClearColor;
        clearValues[Depth].depthStencil.depth = 1.f;

        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);           
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(*resources_.skyboxModel);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);
    }

    void SkyboxPass::onWindowResize()
    {
        createFramebuffer();
    }

    void SkyboxPass::setupRenderPass(const SkyboxPassAttachments& attachments)
    {
        attachments_.addColorAttachment(attachments.color); // 0-color
        attachments_.addColorAttachment(attachments.objectID); // 1-object id
        attachments_.addDepthAttachment(attachments.depth); // 2-depth

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Color] = vks::makeDefaultDescription(*attachments.color, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[ObjectID] = vks::makeDefaultDescription(*attachments.objectID, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ColorAttachmentWrite);
        attachmentDescs[Depth] = vks::makeDefaultDescription(*attachments.depth, vks::TextureUsage::DepthAttachmentWrite, vks::TextureUsage::DepthAttachmentWrite);
        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void SkyboxPass::setupDescriptors()
    {
        // Create sampler
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        context_->createSampler(sampler_, samplerInfo);

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void SkyboxPass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void SkyboxPass::preparePipeline()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo();
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;

        // Set vertex input attachments
        std::vector<vkglTF::VertexComponent> components = { vkglTF::VertexComponent::Position };
        pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState(components);
        
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
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/Scene/Skybox.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Scene/Skybox.frag.spv");

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
    *   FloorPass
    */
    void FloorPass::create(vks::VulkanContext* context, const FloorPassAttachments& attachments)
    {
        context_ = context;

        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void FloorPass::setResources(const FloorPassResources& resources)
    {
        resources_ = resources;

        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorImageInfo floorDescInfo = vks::descriptorImageInfo(*resources_.floorTexture, vks::TextureUsage::ShaderSampledRead, &sampler_);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &floorDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FloorPass::record(vks::CommandList& commandList)
    {
        vks::CommandContext commandContext;

        for(const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        commandContext.useTexture(*attachments_.depthAttachment().texture, vks::TextureUsage::DepthAttachmentWrite);
        
        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::ShaderRead);
        commandContext.useTexture(*resources_.floorTexture, vks::TextureUsage::ShaderSampledRead);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_.getExtent();
        uint32_t width = extent.width;
        uint32_t height = extent.height;

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[Color].color = DefaultClearColor;

        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);              
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(*resources_.floorModel);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);
    }

    void FloorPass::onWindowResize()
    {
        createFramebuffer();
    }

    void FloorPass::setupRenderPass(const FloorPassAttachments& attachments)
    {
        attachments_.addColorAttachment(attachments.position); // 0-position
        attachments_.addColorAttachment(attachments.color); // 1-color
        attachments_.addColorAttachment(attachments.objectID); // 2-object id
        attachments_.addDepthAttachment(attachments.depth); // 3-depth

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Position] = vks::makeDefaultDescription(*attachments.position, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[Color] = vks::makeDefaultDescription(*attachments.color, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[ObjectID] = vks::makeDefaultDescription(*attachments.objectID, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[Depth] = vks::makeDefaultDescription(*attachments.depth, vks::TextureUsage::DepthAttachmentWrite, vks::TextureUsage::DepthAttachmentRead);
        attachmentDescs[ObjectID].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachmentDescs[Depth].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void FloorPass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void FloorPass::setupDescriptors()
    {
        // Create sampler
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        context_->createSampler(sampler_, samplerInfo);

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FloorPass::preparePipeline()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		
        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo();
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;

        // Set vertex input attachments
        std::vector<vkglTF::VertexComponent> components = {
            vkglTF::VertexComponent::Position,
            vkglTF::VertexComponent::UV
        };
        pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState(components);
        
        // Create pipeline_ layout
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
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/Scene/Floor.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Scene/Floor.frag.spv");

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
    *   Scene Composite pass
    */
    void SceneCompositePass::create(vks::VulkanContext* context, const SceneCompositePassAttachments& attachments)
    {
        context_ = context;

        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void SceneCompositePass::setResources(const SceneCompositePassResources& resources)
    {
        resources_ = resources;

        VkDescriptorImageInfo skyBoxColorDescInfo = vks::descriptorImageInfo(*resources_.skyBoxColor, vks::TextureUsage::ShaderSampledRead, &skyboxSampler_);
        VkDescriptorImageInfo floorColorDescInfo = vks::descriptorImageInfo(*resources_.floorColor, vks::TextureUsage::ShaderSampledRead, &floorSampler_);
        VkDescriptorImageInfo objectIDDescInfo = vks::descriptorImageInfo(*resources_.objectID, vks::TextureUsage::ShaderSampledRead);
        
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &skyBoxColorDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &floorColorDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2, &objectIDDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void SceneCompositePass::record(vks::CommandList& commandList)
    {
        vks::CommandContext commandContext;

        for(const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        
        commandContext.useTexture(*resources_.skyBoxColor , vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.floorColor, vks::TextureUsage::ShaderSampledRead);
        commandContext.useTexture(*resources_.objectID, vks::TextureUsage::ShaderSampledRead);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_.getExtent();
        uint32_t width = extent.width;
        uint32_t height = extent.height;

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[Color].color = DefaultClearColor;
        
        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);             
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(3, 1, 0, 0);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);
    }

    void SceneCompositePass::onWindowResize()
    {
        createFramebuffer();
    }

    void SceneCompositePass::setupRenderPass(const SceneCompositePassAttachments& attachments)
    {
        attachments_.addColorAttachment(attachments.color); // 0-color    

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Color] = vks::makeDefaultDescription(*attachments.color, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void SceneCompositePass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void SceneCompositePass::setupDescriptors()
    {
        // Create sampler
        VkSamplerCreateInfo samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        context_->createSampler(skyboxSampler_, samplerInfo);

        samplerInfo = vks::samplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        context_->createSampler(floorSampler_, samplerInfo);

        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 2)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void SceneCompositePass::preparePipeline()
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
        uint32_t colorCount = attachments_.getColorAttachmentCount();
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(colorCount, blendAttachmentState); 
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(colorCount, blendAttachmentStates.data());
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

        // Set shader stages
        std::string shaderPath = getShaderBasePath();

        vks::ShaderModule shaderModules[2];
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/FullScreen.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Scene/Composite.frag.spv");

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