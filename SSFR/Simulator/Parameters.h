#pragma once

#include "Constants.h"
#include <glm/glm.hpp>


namespace ssfr
{
    inline constexpr uint32_t ParticleCountXZ = 40;
    inline constexpr uint32_t ParticleCountY = 40;
    inline constexpr float SupportRadius = 4.f * ParticleRadius;
    inline constexpr float DomainSizeXZ = ParticleCountXZ * ParticleRadius * 8.f;
    inline constexpr float DomainSizeY = (ParticleCountY + 120) * ParticleRadius * 2.f;
    inline constexpr uint32_t ParticleCount = ParticleCountXZ * ParticleCountXZ * ParticleCountY;

    inline constexpr float Dt = 0.0016f;
    inline constexpr float Volume0 = 6.4f  * ParticleRadius * ParticleRadius * ParticleRadius;
    inline constexpr float Mass0 = Volume0 * Density0;
    inline const glm::vec3 Gravity = { 0.f, -9.81f, 0.f };

    inline constexpr uint32_t MaxNeighborCount = 256;
}