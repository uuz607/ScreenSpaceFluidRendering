#pragma once

#include "VulkanglTFModel.h"
#include "Parameters.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"

#include "Passes/ScenePasses.h"
#include "Passes/FluidPasses.h"
#include "Passes/LightingPasses.h"

class Camera;

namespace vks {
    class VulkanContext;
    class CommandList;
}

namespace ssfr
{
    struct RendererResources
    {
        vks::DeviceBuffer<glm::vec4>* position;
    };

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer() = default;    

        void create(vks::VulkanContext* context, std::vector<vks::Texture*> swapChainImages);
        void setResources(const RendererResources& resources);
        void draw(vks::CommandList& commandList, uint32_t index);
        void updateUniformBuffers(const Camera& camera);
        void onWindowResize(uint32_t width, uint32_t height);

    private:
        void loadSceneAssets();
        void prepareUniformBuffers(uint32_t width, uint32_t height);
        void createAttachments(uint32_t width, uint32_t height);
        void createPasses(std::vector<vks::Texture*> swapChainImages);

        void generateSceneGeometry(vks::CommandList& commandList);
        void generateFluidGeometry(vks::CommandList& commandList);
        void generateCaustics(vks::CommandList& commandList);
        void renderWaterEffects(vks::CommandList& commandList, uint32_t index);

    private:
        // Assets
        struct SceneAssets
        {
            struct {
                vkglTF::Model floor;
                vkglTF::Model skybox;
            }model;

            struct {
                vks::Texture floor;
                vks::Texture skybox;
            }texture;
        };

        // Uniform buffers
        struct UniformBuffers
        {
            vks::UniformBuffer<CameraParams> cameraParams;
            vks::UniformBuffer<FluidParams> fluidParams;
        };

        // Attachments
        struct RenderTargets
        {
            struct {
                vks::Texture depth;
                vks::Texture objectID;
                vks::Texture skyboxColor;
                vks::Texture floorPos;
                vks::Texture floorColor;
                vks::Texture baseColor;
            }scene;

            struct {
                vks::Texture depth;
                vks::Texture color;
                vks::Texture smoothedDepth;
                vks::Texture valid;
                vks::Texture thickness;
                vks::Texture viewNormal;
            }fluid;
        };
        
        // Passes
        struct RenderPasses
        {
            struct {
                SkyboxPass skybox;
                FloorPass floor;
                SceneCompositePass composite;
            }scene;

            struct {
                FluidDepthPass depth;
                FluidDepthSmoothPass depthSmooth;
                FluidThicknessPass thickness;
                FluidNormalPass normal;
                FluidNormalRepairPass normalRepair;
            }fluid;

            struct {
                FluidLightingPass fluid;
                ResolvePass resolve;
            }lighting;
        };

    private:
        vks::VulkanContext* context_ = nullptr;

        SceneAssets assets_;

        UniformBuffers uniformBuffers_;
        RenderTargets renderTargets_;

        RenderPasses passes_;
        RendererResources resources_;
    };
}