#pragma once

#include <glm/glm.hpp>
#include <array>
#include <vulkan/vulkan.h>

namespace ssfr
{
    struct CameraParams
    {
        // Matrices
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 projView;

        glm::mat4 invView; 
        glm::mat4 invProj;
        glm::mat4 viewTranspose;
        
        // Position
        glm::vec4 position;
    };

    struct FluidParams
    {
        glm::mat4 model;
        glm::vec4 color;

        std::array<int32_t, 2> screenSize;
        float pointSize;
        float thicknessFactor;
    };

    inline constexpr VkClearColorValue DefaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.f } };
}