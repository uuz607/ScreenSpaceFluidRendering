#pragma once

#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanSampler.h"
#include "Renderer/Parameters.h"

namespace vkglTF 
{
    class Model;
}

namespace vks {
    class CommandList;
    class Texture;
    
    template<typename T>
    class UniformBuffer;
}

namespace ssfr
{   
    struct CameraParams;

    /**
    * SkyboxPass
    *
    * A graphics pass responsible for environment background rendering.
    * It renders a cube mesh textured with a cubemap and writes to
    * scene color and depth attachments.
    *
    * Acts as the first stage of scene rendering before opaque geometry
    * and fluid compositing.
    */
    struct SkyboxPassAttachments
    {
        vks::Texture* color = nullptr;
        vks::Texture* objectID = nullptr;
        vks::Texture* depth = nullptr;
    };

    struct SkyboxPassResources
    {
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;
        vkglTF::Model* skyboxModel = nullptr;
        vks::Texture* skyboxTexture = nullptr;
    };

    class SkyboxPass
    {
    public:
        SkyboxPass() = default;
        ~SkyboxPass() = default;  

        void create(vks::VulkanContext* context, const SkyboxPassAttachments& attachments);
        void setResources(const SkyboxPassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();

    private:
        void setupRenderPass(const SkyboxPassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline();
    
    private:
        enum AttachmentID : uint8_t
        {
            Color,
            ObjectID,
            Depth
        };

    private:
        vks::VulkanContext* context_  = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::Sampler sampler_;
        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        SkyboxPassResources resources_;
    };


    /**
    * FloorPass
    *
    * A scene geometry pass that renders the floor mesh and writes
    * geometric and shading outputs to multiple render targets.
    */
    struct FloorPassAttachments
    {
        vks::Texture* position = nullptr;
        vks::Texture* color = nullptr;
        vks::Texture* objectID = nullptr;
        vks::Texture* depth = nullptr;
    };

    struct FloorPassResources
    {
        vks::UniformBuffer<CameraParams>* cameraParams = nullptr;
        vkglTF::Model* floorModel = nullptr;
        vks::Texture* floorTexture = nullptr;
    };

    class FloorPass
    {
    public:
        FloorPass() = default;
        ~FloorPass() = default;

        void create(vks::VulkanContext* context, const FloorPassAttachments& attachments);
        void setResources(const FloorPassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();

    private:
        void setupRenderPass(const FloorPassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline();
    
    private:
        enum AttachmentID : uint8_t
        {
            Position,
            Color,
            ObjectID,
            Depth
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::Sampler sampler_;
        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        FloorPassResources resources_;
    };
    
    
    /**
    * SceneCompositePass
    *
    * A full-screen pass that merges scene sub-layers into a single
    * base color buffer. Acts as the final scene stage before fluid
    * shading and compositing.
    */
    struct SceneCompositePassAttachments
    {
        vks::Texture* color = nullptr;
    };

    struct SceneCompositePassResources
    {
        vks::Texture* skyBoxColor = nullptr;
        vks::Texture* floorColor = nullptr;
        vks::Texture* objectID = nullptr;
    };

    class SceneCompositePass
    {
    public:
        SceneCompositePass() = default;
        ~SceneCompositePass() = default;

        void create(vks::VulkanContext* context, const SceneCompositePassAttachments& attachments);
        void setResources(const SceneCompositePassResources& resources);
        void record(vks::CommandList& commandList);
        void onWindowResize();

    private:
        void setupRenderPass(const SceneCompositePassAttachments& attachments);
        void createFramebuffer();
        void setupDescriptors();
        void preparePipeline(); 
    
    private:
        enum AttachmentID : uint8_t
        {
            Color
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        vks::Attachments attachments_;
        vks::RenderPass renderPass_;
        vks::Framebuffer framebuffer_;

        vks::Sampler floorSampler_;
        vks::Sampler skyboxSampler_;
        vks::DescriptorSet descriptorSet_;
        vks::Pipeline pipeline_;

        SceneCompositePassResources resources_;
    };
}