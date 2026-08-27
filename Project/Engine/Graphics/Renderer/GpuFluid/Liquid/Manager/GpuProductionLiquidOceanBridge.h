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

/// Gerstner波やFFT Oceanなどの水面実装をローカル液体シミュレーションへ接続するためのProvider契約。
class IGpuProductionLiquidOceanProvider
{
public:
    virtual ~IGpuProductionLiquidOceanProvider() = default;
    [[nodiscard]] virtual GpuProductionLiquidOceanSample SampleOcean(const Vector3& worldPosition) const = 0;
    virtual void SubmitLiquidFeedback(const Vector3& worldPosition, const Vector3& impulse, float radius)
    {
        (void)worldPosition;
        (void)impulse;
        (void)radius;
    }
};

struct GpuProductionLiquidOceanBridgeSettings
{
    bool enabled = false;
    float localSimulationRadius = 18.0f;
    float blendBand = 3.0f;
    float particleSpawnThreshold = 0.35f;
    float feedbackStrength = 0.20f;
    float velocityCoupling = 3.0f;
    float surfaceAttraction = 6.0f;
    float maxVelocityCorrection = 4.0f;
    float feedbackRadius = 2.0f;
};

} // namespace Ken4lowEngine
