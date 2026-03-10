#pragma once

#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanSampler.h"
#include "Renderer/Parameters.h"

#include <glm/glm.hpp>

namespace vks {
    class CommandList;
    class Texture;
    
    template<typename T>
    class UniformBuffer;

    template<typename T>
    class DeviceBuffer;
}

namespace  ssfr
{
    /**
    * FluidLightingPass
    *
    * Performs screen-space fluid shading and produces the final
    * shaded fluid color and depth buffers.
    */
    struct FluidLightingPassAttachments
    {
        vks::Texture* color = nullptr;
        vks::Texture* depth = nullptr;
    };

    struct FluidLightingPassResources
    { 
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;

        struct {
            vks::Texture* skybox = nullptr; 
            vks::Texture* color = nullptr;
        }scene;

        struct {
            vks::UniformBuffer<FluidParams>* params = nullptr;
            vks::DeviceBuffer<glm::vec4>* position = nullptr;
            vks::Texture* viewNormal = nullptr;
            vks::Texture* thickness = nullptr;        
            vks::Texture* smoothedDepth = nullptr;
        }fluid;
    };

    class FluidLightingPass
    {
    public:
        FluidLightingPass() = default;    
        ~FluidLightingPass() = default;

        void create(vks::VulkanContext* context, const FluidLightingPassAttachments& attachments);
        void setResources(const FluidLightingPassResources& resources); 
        void record(vks::CommandList& commandList);
        void onWindowResize();
        
    private:
        void setupRenderPass(const FluidLightingPassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline();

    private:
        enum AttachmentID : uint8_t
        {
            Color,
            Depth
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::Sampler sampler_;
        vks::Sampler skyboxSampler_;
        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FluidLightingPassResources resources_;
    };


    /**
    * ResolvePass
    *
    * Performs final composition between scene and fluid rendering results.
    * This pass blends fluid color into the scene based on depth comparison
    * and fluid validity mask, producing the final frame output.
    */
    struct ResolvePassResources
    {
        struct {
            vks::Texture* color = nullptr;
            vks::Texture* depth = nullptr;
        }scene;

        struct Fluid
        {
            vks::Texture* color = nullptr;
            vks::Texture* depth = nullptr;
            vks::Texture* valid = nullptr;
        }fluid;
    };

    class ResolvePass
    {
    public:
        ResolvePass() = default;
        ~ResolvePass() = default;

        void create(vks::VulkanContext* context, std::vector<vks::Texture*> swapChainImages);
        void setResources(const ResolvePassResources& resources);
        void record(vks::CommandList& commandList, uint32_t index);
        void onWindowResize();

    private:
        void setupRenderPass(std::vector<vks::Texture*> swapChainImages);
        void createFramebuffer();
        void setupDescriptors(std::vector<vks::Texture*> swapChainImages);
        void preparePipeline();

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::RenderPass renderPass_;
        vks::Sampler sampler_;
        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        std::vector<vks::Attachments> attachments_;
        std::vector<vks::Framebuffer> framebuffers_;
        
        ResolvePassResources resources_;
    };
}