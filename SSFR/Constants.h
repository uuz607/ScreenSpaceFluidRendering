#pragma once

#include <glm/glm.hpp>

namespace ssfr 
{
    inline constexpr uint32_t ThreadGroupSizeX1D = 256;
    inline constexpr uint32_t ThreadGroupSizeX2D = 16;
    inline constexpr uint32_t ThreadGroupSizeY2D = 16;

    inline constexpr float ParticleRadius = 0.01f;
    inline constexpr float Density0 = 1000.f;

    inline constexpr uint32_t WindowWidth = 1280;
    inline constexpr uint32_t WindowHeight = 720;
}