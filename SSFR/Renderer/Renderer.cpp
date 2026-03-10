#include "Renderer.h"
#include "VulkanContext.h"
#include "VulkanCommand.h"
#include "AssetLoader.h"
#include "Constants.h"
#include "VulkanTools.h"
#include "camera.hpp"

namespace ssfr
{
    // Fluid constants
    inline constexpr float MinimumDensityScaler = 0.2f;
    inline constexpr float ThicknessScaler = 0.05f;
    inline constexpr float PointSize = ParticleRadius * 3.f;
    // rgb(66, 132, 244)
    inline const glm::vec4 Color = { 66.0f / 255.0f, 132.0f / 255.0f, 244.0f / 255.0f, 1.f };

    /**
    *   Renderer
    */
    void Renderer::create(vks::VulkanContext* context, std::vector<vks::Texture*> swapChainImages)
    {
        context_ = context;

        loadSceneAssets();
        prepareUniformBuffers(WindowWidth, WindowHeight);
        createAttachments(WindowWidth, WindowHeight);
        createPasses(swapChainImages);
    }

    void Renderer::draw(vks::CommandList& commandList, uint32_t index)
    {
        generateSceneGeometry(commandList);
        generateFluidGeometry(commandList);
        renderWaterEffects(commandList, index);
    }

    void Renderer::updateUniformBuffers(const Camera& camera)
    {
        uniformBuffers_.cameraParams.data.view = camera.matrices.view;
        uniformBuffers_.cameraParams.data.proj = camera.matrices.perspective;
        uniformBuffers_.cameraParams.data.projView = camera.matrices.perspective * camera.matrices.view;
        uniformBuffers_.cameraParams.data.invView = glm::inverse(camera.matrices.view);
        uniformBuffers_.cameraParams.data.invProj = glm::inverse(camera.matrices.perspective);
        uniformBuffers_.cameraParams.data.position = glm::vec4(camera.position, 1.0f);
        uniformBuffers_.cameraParams.data.viewTranspose = glm::transpose(camera.matrices.view);

        uniformBuffers_.cameraParams.update();
    }

    void Renderer::onWindowResize(uint32_t width, uint32_t height)
    {
        // Recreate render targets
        createAttachments(width, height);

        passes_.scene.skybox.onWindowResize();
        passes_.scene.floor.onWindowResize();
        passes_.scene.composite.onWindowResize();

        passes_.fluid.depth.onWindowResize();
        passes_.fluid.depthSmooth.onWindowResize();
        passes_.fluid.normalRepair.onWindowResize();
        passes_.fluid.thickness.onWindowResize();

        passes_.lighting.fluid.onWindowResize();
        passes_.lighting.resolve.onWindowResize();
        
        // Update uniform buffers with new screen size
        uniformBuffers_.fluidParams.data.screenSize = { (int32_t)width, (int32_t)height };
        uniformBuffers_.fluidParams.update();
    }


    /**
    *   @brief Load all scene assets_ required for rendering and create corresponding Vulkan resources. 
    *          This includes loading models and textures, and preparing uniform buffers.
    */
    void Renderer::loadSceneAssets()
    {
        std::string assetPath = getAssetPath();

        // Load glTF models
        uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
        vks::AssetLoader::loadGlTFModel(*context_, assets_.model.skybox, glTFLoadingFlags, assetPath + "Skybox/Cube.gltf");
        vks::AssetLoader::loadGlTFModel(*context_, assets_.model.floor, glTFLoadingFlags, assetPath + "Floor/Plane.gltf");

        // Load ktx textures
        vks::AssetLoader::loadKtxTexture(
            *context_, 
            assets_.texture.skybox, 
            vks::TextureType::TextureCube, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            assetPath + "Skybox/Skybox.ktx");
        
        vks::AssetLoader::loadKtxTexture(
            *context_, 
            assets_.texture.floor, 
            vks::TextureType::Texture2D, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            assetPath + "Floor/Floor.ktx");
    }

    void Renderer::prepareUniformBuffers(uint32_t width, uint32_t height)
    {
        context_->createUniformBuffer(uniformBuffers_.cameraParams);
        context_->createUniformBuffer(uniformBuffers_.fluidParams);

        uniformBuffers_.cameraParams.map();
        uniformBuffers_.fluidParams.map();

        // Initialize uniform buffers with default values
        uniformBuffers_.fluidParams.data.model = glm::scale(glm::mat4(1.0f), glm::vec3(-1.0f, -1.0f, -1.0f));
        uniformBuffers_.fluidParams.data.color = Color;
        uniformBuffers_.fluidParams.data.pointSize = PointSize;
        uniformBuffers_.fluidParams.data.thicknessFactor = ThicknessScaler;
        uniformBuffers_.fluidParams.data.screenSize = { (int32_t)width, (int32_t)height };

        uniformBuffers_.fluidParams.update();
    }

    void Renderer::createAttachments(uint32_t width, uint32_t height)
    {
        // Scene geometry renderTargets_
        {
            vks::TextureCreateInfo textureInfo;
            textureInfo.width = width;
            textureInfo.height = height;

            textureInfo.format = context_->depthFormat();
            textureInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.scene.depth, textureInfo);

            textureInfo.format = VK_FORMAT_R8_SINT;
            textureInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.scene.objectID, textureInfo);

            textureInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            textureInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.scene.skyboxColor, textureInfo);
            context_->createTexture(renderTargets_.scene.floorPos, textureInfo);
            context_->createTexture(renderTargets_.scene.floorColor, textureInfo);
            context_->createTexture(renderTargets_.scene.baseColor, textureInfo);
        }
        
        // Fluid geometry renderTargets_
        {
            vks::TextureCreateInfo textureInfo;
            textureInfo.width = width;
            textureInfo.height = height;

            textureInfo.format = context_->depthFormat();
            textureInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.fluid.depth, textureInfo);

            textureInfo.format = VK_FORMAT_R32_SFLOAT;
            textureInfo.usage =  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.fluid.smoothedDepth, textureInfo);

            textureInfo.format = VK_FORMAT_R8_SINT;
            textureInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.fluid.valid, textureInfo);

            textureInfo.format = VK_FORMAT_R32_SFLOAT;
            textureInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.fluid.thickness, textureInfo);

            textureInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            textureInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            context_->createTexture(renderTargets_.fluid.viewNormal, textureInfo);

            textureInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
            textureInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            context_->createTexture(renderTargets_.fluid.color, textureInfo);
        }
    }

    
    void Renderer::createPasses(std::vector<vks::Texture*> swapChainImages)
    {
        // Configure resources for background scene rendering, including skybox, floor geometry and final scene composition.
        {
            // Skybox pass: renders environment background using camera parameters
            SkyboxPassAttachments skyboxPassAttachments = { 
                &renderTargets_.scene.skyboxColor, 
                &renderTargets_.scene.objectID,
                &renderTargets_.scene.depth 
            };
            passes_.scene.skybox.create(context_, skyboxPassAttachments);

            // Floor pass: renders ground plane with camera and texture inputs
            FloorPassAttachments floorPassAttachments = {
                &renderTargets_.scene.floorPos,
                &renderTargets_.scene.floorColor,
                &renderTargets_.scene.objectID,
                &renderTargets_.scene.depth
            };
            passes_.scene.floor.create(context_, floorPassAttachments);

            // Scene composite pass: merges skybox and floor color targets and writes object ID buffer for later usage
            SceneCompositePassAttachments compositePassAttachments = { &renderTargets_.scene.baseColor };
            passes_.scene.composite.create(context_, compositePassAttachments);
        }

        // Generate screen-space fluid representation including depth, smoothed depth, thickness and reconstructed normals.
        {
            // Fluid depth pass: render particle spheres into depth buffer
            FluidDepthPassAttachments depthPassAttachments = {
                &renderTargets_.fluid.depth,
                &renderTargets_.fluid.smoothedDepth,
                &renderTargets_.fluid.valid
            };
            passes_.fluid.depth.create(context_, depthPassAttachments);

            // Depth smoothing pass (bilateral blur): produces continuous depth field and valid mask
            passes_.fluid.depthSmooth.create(context_);

            // Thickness pass: accumulate particle thickness for absorption
            FluidThicknessPassAttachments thicknessPassAttachments = {
                &renderTargets_.fluid.thickness,
                &renderTargets_.fluid.depth
            };
            passes_.fluid.thickness.create(context_, thicknessPassAttachments);

            // Normal reconstruction pass: reconstruct view-space normals from smoothed depth         
            passes_.fluid.normal.create(context_);

            // Normal repair pass: fix discontinuities or invalid regions in reconstructed normals
            passes_.fluid.normalRepair.create(context_);
        }

        // Perform fluid shading (refraction + reflection) and merge fluid result into scene.
        {
            // Fluid lighting pass: performs refraction, reflection and absorption using reconstructed normals, thickness and scene color buffer
            FluidLightingPassAttachments fluidLightingPassAttachments = {
                &renderTargets_.fluid.color,
                &renderTargets_.fluid.depth
            };
            passes_.lighting.fluid.create(context_, fluidLightingPassAttachments);

            // Resolve pass: composite fluid color and depth with scene buffers using valid mask to ensure correct blending
            passes_.lighting.resolve.create(context_, swapChainImages);
        }
    }

    void Renderer::setResources(const RendererResources& resources)
    {
        resources_ = resources;

        // Bind required GPU resources to each sence rendering pass
        {
            SkyboxPassResources skyboxPassResources = { 
                &uniformBuffers_.cameraParams, 
                &assets_.model.skybox, 
                &assets_.texture.skybox 
            };
            passes_.scene.skybox.setResources(skyboxPassResources);

            FloorPassResources floorPassResources = { 
                &uniformBuffers_.cameraParams, 
                &assets_.model.floor, 
                &assets_.texture.floor 
            };
            passes_.scene.floor.setResources(floorPassResources);

            SceneCompositePassResources compositePassResources = { 
                &renderTargets_.scene.skyboxColor, 
                &renderTargets_.scene.floorColor, 
                &renderTargets_.scene.objectID 
            };
            passes_.scene.composite.setResources(compositePassResources);
        }
        
        // Bind required GPU resources to each fluid rendering pass
        {
            FluidDepthPassResources depthPassResources = { 
                &uniformBuffers_.cameraParams,
                &uniformBuffers_.fluidParams,
                resources_.position
            };
            passes_.fluid.depth.setResources(depthPassResources);

            FluidDepthSmoothPassResources depthSmoothPassResources = { 
                &uniformBuffers_.fluidParams,
                &renderTargets_.fluid.smoothedDepth,
                &renderTargets_.fluid.valid
            };
            passes_.fluid.depthSmooth.setResources(depthSmoothPassResources);

            FluidThicknessPassResources thicknessPassResources = {
                &uniformBuffers_.cameraParams,
                &uniformBuffers_.fluidParams,
                resources_.position
            };
            passes_.fluid.thickness.setResources(thicknessPassResources);
            
            FluidNormalPassResources normalPassResources = {
                &uniformBuffers_.cameraParams,
                &uniformBuffers_.fluidParams,
                &renderTargets_.fluid.smoothedDepth,
                &renderTargets_.fluid.viewNormal
            };
            passes_.fluid.normal.setResources(normalPassResources);

            FluidNormalRepairPassResources normalRepairPassResources = { 
                &uniformBuffers_.fluidParams, 
                &renderTargets_.fluid.viewNormal
            };
            passes_.fluid.normalRepair.setResources(normalRepairPassResources);
        }

        // Bind required GPU resources to each fluid lighting pass
        {
            FluidLightingPassResources fluidLightingPassResources;
            fluidLightingPassResources.cameraParams = &uniformBuffers_.cameraParams;
            fluidLightingPassResources.scene.skybox = &assets_.texture.skybox;
            fluidLightingPassResources.scene.color = &renderTargets_.scene.baseColor;
            fluidLightingPassResources.fluid.params = &uniformBuffers_.fluidParams;
            fluidLightingPassResources.fluid.position = resources_.position;
            fluidLightingPassResources.fluid.viewNormal = &renderTargets_.fluid.viewNormal;
            fluidLightingPassResources.fluid.thickness = &renderTargets_.fluid.thickness;
            fluidLightingPassResources.fluid.smoothedDepth = &renderTargets_.fluid.smoothedDepth;
            passes_.lighting.fluid.setResources(fluidLightingPassResources);

            ResolvePassResources resolvePassResources;
            resolvePassResources.scene.color = &renderTargets_.scene.baseColor;
            resolvePassResources.scene.depth = &renderTargets_.scene.depth;
            resolvePassResources.fluid.color = &renderTargets_.fluid.color;
            resolvePassResources.fluid.depth = &renderTargets_.fluid.depth;
            resolvePassResources.fluid.valid = &renderTargets_.fluid.valid;
            passes_.lighting.resolve.setResources(resolvePassResources);
        }
    }

    void Renderer::generateSceneGeometry(vks::CommandList& commandList)
    {
        passes_.scene.skybox.record(commandList);
        passes_.scene.floor.record(commandList);
        passes_.scene.composite.record(commandList);
    }

    void Renderer::generateFluidGeometry(vks::CommandList& commandList)
    {
        passes_.fluid.depth.record(commandList);
        passes_.fluid.depthSmooth.record(commandList);
        passes_.fluid.normal.record(commandList);
        passes_.fluid.normalRepair.record(commandList);        
        passes_.fluid.thickness.record(commandList);
    }

    void Renderer::renderWaterEffects(vks::CommandList& commandList, uint32_t index)
    {
        passes_.lighting.fluid.record(commandList);
        passes_.lighting.resolve.record(commandList, index);
    }
}