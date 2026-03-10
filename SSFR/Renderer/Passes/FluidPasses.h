#pragma once

#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "Renderer/Parameters.h"

#include <glm/glm.hpp>
#include <array>
    

namespace vks {
    class CommandList;
    class Texture;
    
    template<typename T>
    class UniformBuffer;

    template<typename T>
    class DeviceBuffer;
}

namespace ssfr
{
    /**
    * FluidDepthPass
    *
    * Renders particle spheres into a screen-space depth buffer to
    * capture the front-most fluid surface depth.
    *
    * This pass converts particle positions into a discrete depth field
    * that represents the fluid surface prior to smoothing.
    */
    struct FluidDepthPassAttachments
    {
        vks::Texture* depth = nullptr;
        vks::Texture* smoothedDepth = nullptr;
        vks::Texture* valid = nullptr;
    };

    struct FluidDepthPassResources
    {
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;
        vks::UniformBuffer<FluidParams>* fluidParams = nullptr;
        vks::DeviceBuffer<glm::vec4>* position = nullptr;
    };

    class FluidDepthPass
    {
    public:
        FluidDepthPass() = default;
        ~FluidDepthPass() = default;

        void create(vks::VulkanContext* context, const FluidDepthPassAttachments& attachments);
        void setResources(const FluidDepthPassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();
    
    private:
        void setupRenderPass(const FluidDepthPassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline();
    
    private:
        enum AttachmentID : uint8_t
        {
            Valid,
            SmoothedDepth,
            Depth
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FluidDepthPassResources resources_;
    };


    /**
    * FluidDepthSmoothPass
    *
    * Applies bilateral filtering to the discrete fluid depth buffer
    * in order to generate a continuous and smooth depth field.
    *
    * This pass bridges the gap between individual particle spheres
    * and a coherent screen-space fluid surface representation.
    */
    struct FluidDepthSmoothPassResources
    {
        vks::UniformBuffer<FluidParams>* fluidParams = nullptr;
        vks::Texture* smoothedDepth = nullptr;
        vks::Texture* valid = nullptr;
    };

    class FluidDepthSmoothPass
    {
    public:
        FluidDepthSmoothPass() = default;
        ~FluidDepthSmoothPass() = default;

        void create(vks::VulkanContext* context);
        void setResources(const FluidDepthSmoothPassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();

    private:
        void createDepthTexture();
        void setupDescriptors();
        void preparePipeline();

    private:
        enum FilterDirection : int32_t
        {
            Horizontal,
            Vertical
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        struct PushConsts
        {
            int32_t axis;
            int32_t kernelRadius;
        }pushConsts_;

        vks::Texture depthTexture_;
        
        std::array<vks::DescriptorSet, 2> descriptorSets_;
        vks::Pipeline pipeline_;

        FluidDepthSmoothPassResources resources_;
    };


    /**
    * FluidThicknessPass (RenderGraph Node)
    *
    * Accumulates per-pixel fluid thickness in screen space.
    *
    * This pass integrates the contribution of particle spheres along
    * the view direction to approximate optical path length through
    * the fluid, which is later used for absorption and attenuation.
    */
    struct FluidThicknessPassAttachments
    {
        vks::Texture* thickness = nullptr;
        vks::Texture* depth = nullptr;
    };

    struct FluidThicknessPassResources
    {
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;
        vks::UniformBuffer<FluidParams>* fluidParams = nullptr;
        vks::DeviceBuffer<glm::vec4>* position = nullptr;
    };

    class FluidThicknessPass
    {
    public:
        FluidThicknessPass() = default;
        ~FluidThicknessPass() = default;

        void create(vks::VulkanContext* context, const FluidThicknessPassAttachments& attachments);
        void setResources(const FluidThicknessPassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();

    private:
        void setupRenderPass(const FluidThicknessPassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline();

    private:
        enum AttachmentID : uint8_t
        {
            Thickness,
            Depth
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FluidThicknessPassResources resources_;
    };


    /**
    * FluidNormalPass
    *
    * Reconstructs view-space normals from the smoothed fluid depth field.
    *
    * This pass estimates screen-space gradients of the continuous
    * depth buffer to approximate the fluid surface normal, enabling
    * physically-inspired shading such as refraction and reflection.
    */
    struct FluidNormalPassResources
    {
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;
        vks::UniformBuffer<FluidParams>* fluidParams = nullptr;
        vks::Texture* smoothedDepth = nullptr;
        vks::Texture* normal = nullptr;
    };

    class FluidNormalPass
    {
    public:
        FluidNormalPass() = default;
        ~FluidNormalPass() = default;

        void create(vks::VulkanContext* context);
        void setResources(const FluidNormalPassResources& resources);
        void record(vks::CommandList& commandList);        

    private:
        void setupDescriptors();
        void preparePipeline();

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FluidNormalPassResources resources_;
    };


    /**
    * FluidNormalRepairPass (RenderGraph Node)
    *
    * Post-processes reconstructed fluid normals to repair invalid,
    * discontinuous or unstable regions.
    *
    * This pass improves visual stability by smoothing artifacts
    * caused by sparse sampling, depth discontinuities or reconstruction noise.
    */
    struct FluidNormalRepairPassResources
    {
        vks::UniformBuffer<FluidParams>* fluidParams = nullptr;
        vks::Texture* normal = nullptr;
    };

    class FluidNormalRepairPass
    {
    public:
        FluidNormalRepairPass() = default;
        ~FluidNormalRepairPass() = default;

        void create(vks::VulkanContext* context);
        void setResources(const FluidNormalRepairPassResources& resources);
        void record(vks::CommandList& commandList); 
        void onWindowResize();     

    private:
        void createRepairedTexture();
        void setupDescriptors();
        void preparePipeline();

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Texture repairedTexture_;

        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FluidNormalRepairPassResources resources_;
    };
}