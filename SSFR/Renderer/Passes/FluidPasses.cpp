#include "FluidPasses.h"
#include "VulkanShader.hpp"
#include "AssetLoader.h"
#include "VulkanContext.h"
#include "VulkanCommand.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"
#include "Constants.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"

namespace ssfr
{
    inline constexpr uint32_t SmoothIteration = 8;
    inline constexpr float SmoothKernelRadius = 15.f;

    /**
    *   FluidDepthPass
    */
    void FluidDepthPass::create(vks::VulkanContext* context, const FluidDepthPassAttachments& attachments)
    {
        context_ = context;

        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void FluidDepthPass::setResources(const FluidDepthPassResources& resources)
    {
        resources_ = resources;

        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorBufferInfo fluidDescInfo = resources_.fluidParams->descriptorInfo();
        VkDescriptorBufferInfo positionDescInfo = resources_.position->descriptorInfo();

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &fluidDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &positionDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FluidDepthPass::record(vks::CommandList& commandList)
    {
        vks::CommandContext commandContext;

        for (const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        commandContext.useTexture(*attachments_.depthAttachment().texture, vks::TextureUsage::DepthAttachmentWrite);

        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::UniformRead);
        commandContext.useBuffer(*resources_.fluidParams, vks::BufferUsage::UniformRead);

        // Acquire buffer
        uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
        uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
        if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
        {
            commandContext.acquireBuffer(
                *resources_.position, 
                vks::BufferUsage::ShaderRead, 
                srcQueueFamilyIndex, 
                dstQueueFamilyIndex);
        }
        else 
        {
            commandContext.useBuffer(*resources_.position, vks::BufferUsage::ShaderRead);
        }

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_.getExtent(); 
        uint32_t width = extent.width;
        uint32_t height = extent.height;
        uint32_t particleCount = resources_.position->count();

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[SmoothedDepth].color = { { 1.f, 0.f, 0.f, 0.f} };
        clearValues[Depth].depthStencil.depth = 1.f;

        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);             
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(6, particleCount, 0, 0);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);
    }

    void FluidDepthPass::onWindowResize()
    {
        createFramebuffer();
    }

    void FluidDepthPass::setupRenderPass(const FluidDepthPassAttachments& attachments)
    {   
        attachments_.addColorAttachment(attachments.valid); // 0-valid
        attachments_.addColorAttachment(attachments.smoothedDepth); // 1-smoothed depth
        attachments_.addDepthAttachment(attachments.depth); // 2-depth 

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Valid] = vks::makeDefaultDescription(*attachments.valid, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[SmoothedDepth] = vks::makeDefaultDescription(*attachments.smoothedDepth, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[Depth] = vks::makeDefaultDescription(*attachments.depth, vks::TextureUsage::DepthAttachmentWrite, vks::TextureUsage::DepthAttachmentWrite);
        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void FluidDepthPass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void FluidDepthPass::setupDescriptors()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FluidDepthPass::preparePipeline()
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

        // Set color blend attachments_
        uint32_t colorCount = attachments_.getColorAttachmentCount();
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates(colorCount, blendAttachmentState); 
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(colorCount, blendAttachmentStates.data());
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

        // Set shader stages        
        std::string shaderPath = getShaderBasePath();

        vks::ShaderModule shaderModules[2];
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/Particle.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Fluid/DepthTexture.frag.spv");

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
    *   FluidDepthSmoothPass
    */
    void FluidDepthSmoothPass::create(vks::VulkanContext* context)
    {   
        context_ = context;

        setupDescriptors();
        preparePipeline();
    }

    void FluidDepthSmoothPass::setResources(const FluidDepthSmoothPassResources& resources)
    {
        resources_ = resources;

        createDepthTexture();

        // Write horizontal descriptor sets
        VkDescriptorBufferInfo fluidDescInfo = resources_.fluidParams->descriptorInfo();
        VkDescriptorImageInfo  inDepthDescInfo = vks::descriptorImageInfo(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderRead);
        VkDescriptorImageInfo  outDepthDescInfo = vks::descriptorImageInfo(depthTexture_, vks::TextureUsage::ComputeShaderWrite);
        VkDescriptorImageInfo  validDescInfo = vks::descriptorImageInfo(*resources_.valid, vks::TextureUsage::ComputeShaderRead);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &fluidDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  1, &inDepthDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  2, &outDepthDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  3, &validDescInfo),
        };
        descriptorSets_[Horizontal].updateDescriptorSets(writeDescriptorSets);

        // Write vertical descriptor sets
        inDepthDescInfo = vks::descriptorImageInfo(depthTexture_, vks::TextureUsage::ComputeShaderRead);
        outDepthDescInfo = vks::descriptorImageInfo(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderWrite);   

        descriptorSets_[Vertical].updateDescriptorSets(writeDescriptorSets);
    }

    void FluidDepthSmoothPass::record(vks::CommandList& commandList)
    {
        //
        uint32_t width = resources_.smoothedDepth->width();
        uint32_t height = resources_.smoothedDepth->height();
        uint32_t groupCountX = (width + ThreadGroupSizeX2D -1) / ThreadGroupSizeX2D;
        uint32_t groupCountY = (height + ThreadGroupSizeY2D -1) / ThreadGroupSizeY2D;

        vks::CommandContext commandContext;

        commandContext.useBuffer(*resources_.fluidParams, vks::BufferUsage::ComputeUniformRead);
        commandContext.useTexture(*resources_.valid, vks::TextureUsage::ComputeShaderRead);

        commandList.bindPipeline(pipeline_);

        uint32_t smoothIteration = SmoothIteration;
        float smoothKernelRadius = SmoothKernelRadius;

        float delta = smoothIteration > 1? (smoothKernelRadius - 1.f) / (smoothIteration - 1) : 0.f;
        for (uint32_t i = 0; i < smoothIteration; ++i)
        {
            pushConsts_.axis = Horizontal;
            pushConsts_.kernelRadius = static_cast<int32_t>(smoothKernelRadius);

            commandList.pushConstants(pipeline_, VK_SHADER_STAGE_COMPUTE_BIT, offsetof(PushConsts, axis), sizeof(int32_t), &pushConsts_.axis);
            commandList.pushConstants(pipeline_, VK_SHADER_STAGE_COMPUTE_BIT, offsetof(PushConsts, kernelRadius), sizeof(int32_t), &pushConsts_.kernelRadius);

            // First: filter horizontal
            commandContext.useTexture(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderRead);
            commandContext.useTexture(depthTexture_, vks::TextureUsage::ComputeShaderWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindDescriptorSets(pipeline_, descriptorSets_[Horizontal]);
            commandList.dispatch(groupCountX, groupCountY, 1);

            // Second: filter vertical
            pushConsts_.axis = Vertical;
            commandList.pushConstants(pipeline_, VK_SHADER_STAGE_COMPUTE_BIT, offsetof(PushConsts, axis), sizeof(int32_t), &pushConsts_.axis);

            commandContext.useTexture(depthTexture_, vks::TextureUsage::ComputeShaderRead);
            commandContext.useTexture(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderWrite);

            commandContext.flushBarriers(commandList);
            commandContext.commitTransitions();

            commandList.bindDescriptorSets(pipeline_, descriptorSets_[Vertical]);
            commandList.dispatch(groupCountX, groupCountY, 1);

            // Update kernel radius
            smoothKernelRadius = std::max(1.f, smoothKernelRadius - delta);
        }
    }

    void FluidDepthSmoothPass::onWindowResize()
    {
        createDepthTexture();
    }

    void FluidDepthSmoothPass::createDepthTexture()
    {
        vks::TextureCreateInfo textureInfo;
        textureInfo.width  = resources_.smoothedDepth->width();
        textureInfo.height = resources_.smoothedDepth->height();
        textureInfo.format = VK_FORMAT_R32_SFLOAT;
        textureInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        context_->createTexture(depthTexture_, textureInfo);
    }

    void FluidDepthSmoothPass::setupDescriptors()
    {
        // Create horizontal and vertical descriptor sets 
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_SHADER_STAGE_COMPUTE_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  VK_SHADER_STAGE_COMPUTE_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_SHADER_STAGE_COMPUTE_BIT, 3),
        };
        context_->createDescriptorSet(descriptorSets_[Horizontal], layoutBindings);
        context_->createDescriptorSet(descriptorSets_[Vertical], layoutBindings);
    }

    void FluidDepthSmoothPass::preparePipeline()
    {
        // Create pipeline layout
        VkDescriptorSetLayout descriptorSetLayout = descriptorSets_[Horizontal].layout();
        VkPushConstantRange pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        VkPipelineLayout pipelineLayout = context_->createPipelineLayout(pipelineLayoutInfo);

        VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

        // Set shader stages
        std::string shaderPath = getShaderBasePath() + "Renderer/Fluid/DepthSmooth.comp.spv";

        vks::ShaderModule shaderModule;
        vks::AssetLoader::createShaderModule(*context_, shaderModule, shaderPath);

        VkPipelineShaderStageCreateInfo shaderStage = vks::pipelineShaderStageCreateInfo(shaderModule, VK_SHADER_STAGE_COMPUTE_BIT);
        pipelineCreateInfo.stage = shaderStage;

        context_->createComputePipeline(pipeline_, pipelineCreateInfo);
    }


    /**
    *   FluidThicknessPass
    */
    void FluidThicknessPass::create(vks::VulkanContext* context, const FluidThicknessPassAttachments& attachments)
    {
        context_ = context;
        setupRenderPass(attachments);
        createFramebuffer();
        setupDescriptors();
        preparePipeline();
    }

    void FluidThicknessPass::setResources(const FluidThicknessPassResources& resources)
    {
        resources_ = resources;

        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorBufferInfo fluidDescInfo = resources_.fluidParams->descriptorInfo();
        VkDescriptorBufferInfo positionDescInfo = resources_.position->descriptorInfo();

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &fluidDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &positionDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FluidThicknessPass::record(vks::CommandList& commandList)
    {   
        vks::CommandContext commandContext;
        
        for (const auto& color : attachments_.colorAttachments())
        {
            commandContext.useTexture(*color.texture, vks::TextureUsage::ColorAttachmentWrite);
        }
        commandContext.useTexture(*attachments_.depthAttachment().texture, vks::TextureUsage::DepthAttachmentWrite);

        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::UniformRead);
        commandContext.useBuffer(*resources_.fluidParams, vks::BufferUsage::UniformRead);

        // Acquire buffer
        uint32_t srcQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.compute;
        uint32_t dstQueueFamilyIndex = context_->vulkanDevice()->queueFamilyIndices.graphics;
        if (srcQueueFamilyIndex != dstQueueFamilyIndex) 
        {
            commandContext.acquireBuffer(
                *resources_.position, 
                vks::BufferUsage::ShaderRead, 
                srcQueueFamilyIndex, 
                dstQueueFamilyIndex);
        }
        else 
        {
            commandContext.useBuffer(*resources_.position, vks::BufferUsage::ShaderRead);
        }

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        VkExtent2D extent = attachments_.getExtent(); 
        uint32_t width = extent.width;
        uint32_t height = extent.height;
        uint32_t particleCount = resources_.position->count();

        std::vector<VkClearValue> clearValues(attachments_.getAttachmentCount());
        clearValues[Depth].depthStencil.depth = 1.f;

        commandList.beginRenderPass(renderPass_, framebuffer_, clearValues, width, height);
        commandList.setViewport((float)width, (float)height);
        commandList.setScissor((int32_t)width, (int32_t)height);            
        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.draw(6, particleCount, 0, 0);
        commandList.endRenderPass();

        commandContext.commitAttachmentLayouts(attachments_);
    }

    void FluidThicknessPass::onWindowResize()
    {
        createFramebuffer();
    }

    void FluidThicknessPass::setupRenderPass(const FluidThicknessPassAttachments& attachments)
    {
        attachments_.addColorAttachment(attachments.thickness); // 0-thickness
        attachments_.addDepthAttachment(attachments.depth); // 1-depth

        std::vector<VkAttachmentDescription> attachmentDescs(attachments_.getAttachmentCount());
        attachmentDescs[Thickness] = vks::makeDefaultDescription(*attachments.thickness, vks::TextureUsage::ColorAttachmentWrite, vks::TextureUsage::ShaderSampledRead);
        attachmentDescs[Depth] = vks::makeDefaultDescription(*attachments.depth, vks::TextureUsage::DepthAttachmentWrite, vks::TextureUsage::DepthAttachmentWrite);
        context_->createRenderPass(renderPass_, attachments_, attachmentDescs);
        attachments_.setFinalLayouts(attachmentDescs);
    }

    void FluidThicknessPass::createFramebuffer()
    {
        context_->createFramebuffer(framebuffer_, renderPass_, attachments_);
    }

    void FluidThicknessPass::setupDescriptors()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 2)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FluidThicknessPass::preparePipeline()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS);
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
        VkPipelineColorBlendAttachmentState blendAttachmentState = {};
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

        // Set shader stages
        std::string shaderPath = getShaderBasePath();

        vks::ShaderModule shaderModules[2];
        vks::AssetLoader::createShaderModule(*context_, shaderModules[0], shaderPath + "Renderer/Particle.vert.spv");
        vks::AssetLoader::createShaderModule(*context_, shaderModules[1], shaderPath + "Renderer/Fluid/ThicknessTexture.frag.spv");

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
    *   FluidNormalPass
    */
    void FluidNormalPass::create(vks::VulkanContext* context)
    {
        context_ = context;

        setupDescriptors();
        preparePipeline();
    }

    void FluidNormalPass::setResources(const FluidNormalPassResources& resources)
    {
        resources_ = resources;

        VkDescriptorBufferInfo cameraDescInfo = resources_.cameraParams->descriptorInfo();
        VkDescriptorBufferInfo fluidDescInfo = resources_.fluidParams->descriptorInfo();
        VkDescriptorImageInfo depthDescInfo = vks::descriptorImageInfo(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderRead);
        VkDescriptorImageInfo normalDescInfo = vks::descriptorImageInfo(*resources_.normal, vks::TextureUsage::ComputeShaderWrite);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &cameraDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &fluidDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2, &depthDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3, &normalDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FluidNormalPass::record(vks::CommandList& commandList)
    {
        uint32_t width = resources_.normal->width();
        uint32_t height = resources_.normal->height();
        uint32_t groupCountX = (width + ThreadGroupSizeX2D -1) / ThreadGroupSizeX2D;
        uint32_t groupCountY = (height + ThreadGroupSizeY2D -1) / ThreadGroupSizeY2D;

        vks::CommandContext commandContext;

        // Reconstruct the normal
        commandContext.useBuffer(*resources_.cameraParams, vks::BufferUsage::ComputeUniformRead);
        commandContext.useBuffer(*resources_.fluidParams, vks::BufferUsage::ComputeUniformRead);
        commandContext.useTexture(*resources_.smoothedDepth, vks::TextureUsage::ComputeShaderRead);
        commandContext.useTexture(*resources_.normal, vks::TextureUsage::ComputeShaderWrite);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.dispatch(groupCountX, groupCountY, 1);
    }

    void FluidNormalPass::setupDescriptors()
    {
        // reconstruct descriptor set
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FluidNormalPass::preparePipeline()
    {
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        VkPipelineLayout pipelineLayout = context_->createPipelineLayout(pipelineLayoutInfo);

        VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

        std::string shaderPath = getShaderBasePath() + "Renderer/Fluid/NormalConstruct.comp.spv";

        vks::ShaderModule shaderModule;
        vks::AssetLoader::createShaderModule(*context_, shaderModule, shaderPath);

        VkPipelineShaderStageCreateInfo shaderStage = vks::pipelineShaderStageCreateInfo(shaderModule, VK_SHADER_STAGE_COMPUTE_BIT);
        pipelineCreateInfo.stage = shaderStage;

        context_->createComputePipeline(pipeline_, pipelineCreateInfo);
    }


    /**
    *   Normal repair pass
    */
    void FluidNormalRepairPass::create(vks::VulkanContext* context)
    {
        context_ = context;

        setupDescriptors();
        preparePipeline();
    }

    void FluidNormalRepairPass::setResources(const FluidNormalRepairPassResources& resources)
    {
        resources_ = resources;

        createRepairedTexture();
        
        //
        VkDescriptorBufferInfo fluidDescInfo = resources_.fluidParams->descriptorInfo();
        VkDescriptorImageInfo inNormalDescInfo = vks::descriptorImageInfo(*resources_.normal, vks::TextureUsage::ComputeShaderRead);
        VkDescriptorImageInfo repairedNormalDescInfo = vks::descriptorImageInfo(repairedTexture_, vks::TextureUsage::ComputeShaderWrite);

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &fluidDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, &inNormalDescInfo),
            vks::initializers::writeDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &repairedNormalDescInfo)
        };
        descriptorSet_.updateDescriptorSets(writeDescriptorSets);
    }

    void FluidNormalRepairPass::record(vks::CommandList& commandList)
    {
        uint32_t width = resources_.normal->width();
        uint32_t height = resources_.normal->height();
        uint32_t groupCountX = (width + ThreadGroupSizeX2D -1) / ThreadGroupSizeX2D;
        uint32_t groupCountY = (height + ThreadGroupSizeY2D -1) / ThreadGroupSizeY2D;

        vks::CommandContext commandContext;

        // Reconstruct the normal
        commandContext.useBuffer(*resources_.fluidParams, vks::BufferUsage::ComputeUniformRead);
        commandContext.useTexture(*resources_.normal, vks::TextureUsage::ComputeShaderRead);
        commandContext.useTexture(repairedTexture_, vks::TextureUsage::ComputeShaderWrite);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        commandList.bindPipeline(pipeline_);
        commandList.bindDescriptorSets(pipeline_, descriptorSet_);
        commandList.dispatch(groupCountX, groupCountY, 1);

        // Copy image
        commandContext.useTexture(repairedTexture_, vks::TextureUsage::TransferSrc);
        commandContext.useTexture(*resources_.normal, vks::TextureUsage::TransferDst);

        commandContext.flushBarriers(commandList);
        commandContext.commitTransitions();

        commandList.copyImage(repairedTexture_, *resources_.normal);
    }      

    void FluidNormalRepairPass::onWindowResize()
    {
        createRepairedTexture();
    }

    void FluidNormalRepairPass::createRepairedTexture()
    {
        vks::TextureCreateInfo textureInfo;
        textureInfo.width  = resources_.normal->width();
        textureInfo.height = resources_.normal->height();
        textureInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        textureInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        context_->createTexture(repairedTexture_, textureInfo);
    }

    void FluidNormalRepairPass::setupDescriptors()
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2)
        };
        context_->createDescriptorSet(descriptorSet_, layoutBindings);
    }

    void FluidNormalRepairPass::preparePipeline()
    {
        VkDescriptorSetLayout descriptorSetLayout = descriptorSet_.layout();
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout);
        VkPipelineLayout pipelineLayout = context_->createPipelineLayout(pipelineLayoutInfo);

        VkComputePipelineCreateInfo pipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

        std::string shaderPath = getShaderBasePath() + "Renderer/Fluid/NormalRepair.comp.spv";

        vks::ShaderModule shaderModule;
        vks::AssetLoader::createShaderModule(*context_, shaderModule, shaderPath);

        VkPipelineShaderStageCreateInfo shaderStage = vks::pipelineShaderStageCreateInfo(shaderModule, VK_SHADER_STAGE_COMPUTE_BIT);
        pipelineCreateInfo.stage = shaderStage;

        context_->createComputePipeline(pipeline_, pipelineCreateInfo);
    }
}