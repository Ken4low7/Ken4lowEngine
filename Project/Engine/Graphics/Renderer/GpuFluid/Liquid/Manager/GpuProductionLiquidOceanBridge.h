#pragma once

#include <Vector3.h>

namespace Ken4lowEngine
{

struct GpuProductionLiquidOceanSample
{
    float height = 0.0f;
    Vector3 normal{ 0.0f, 1.0f, 0.0f };
    Vector3 velocity{};
    bool valid = false;
};

/// W10.6: Gerstner/FFT等のOcean実装をLocal Liquidへ接続するための非所有Provider契約。
class IGpuProductionLiquidOceanProvider
{
public:
    virtual ~IGpuProductionLiquidOceanProvider() = default;
    [[nodiscard]] virtual GpuProductionLiquidOceanSample SampleOcean(const Vector3& worldPosition) const = 0;
};

struct GpuProductionLiquidOceanBridgeSettings
{
    bool enabled = false;
    float localSimulationRadius = 18.0f;
    float blendBand = 3.0f;
    float particleSpawnThreshold = 0.35f;
    float feedbackStrength = 0.20f;
};

} // namespace Ken4lowEngine
