#pragma once
#include <string>
#include <Vector3.h>

#include "GpuParticleType.h"
#include "BillboardMode.h"

namespace Ken4lowEngine
{

    struct GpuParticleEmitterAsset
    {
        std::string name;
        std::string textureFilePath;

        Vector3 position{ 0.0f, 0.0f, 0.0f };

        float radius = 0.0f;
        uint32_t loopCount = 0;
        float loopFrequency = 0.0f;
        uint32_t drawType = 0;

        GpuParticleKind kind = GpuParticleKind::Sprite;
        GpuParticleType spriteType = GpuParticleType::Default;
        GpuRibbonType ribbonType = GpuRibbonType::Trail;
        BillboardMode billboardFlags = BillboardMode::Camera;
    };

} // namespace Ken4lowEngine