#pragma once

#include "GpuProductionLiquidOceanBridge.h"
#include "GpuProductionLiquidOceanCoupler.h"
#include "GpuProductionLiquidSecondaryClassifier.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Renderer/GpuSphScreenSpaceFluidRenderer.h"
#include "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

enum class GpuProductionLiquidQualityPreset : uint32_t
{
    Development = 0,
    Interactive,
    High,
    Cinematic,
    Stress,
};

struct GpuProductionLiquidAdaptiveSolverSettings
{
    bool enabled = true;
    uint32_t minDensityIterations = 2;
    uint32_t maxDensityIterations = 8;
    uint32_t minDivergenceIterations = 1;
    uint32_t maxDivergenceIterations = 5;
    uint32_t calmFramesBeforeDownshift = 12;
};

struct GpuProductionLiquidSecondarySettings
{
    bool enabled = true;
    float spraySpeedThreshold = 3.5f;
    float foamSpeedThreshold = 1.5f;
    float freeSurfaceDensityRatio = 0.88f;
    float bubbleDensityRatio = 1.08f;
    uint32_t classificationIntervalFrames = 2;
};

struct GpuProductionLiquidFlipApicSettings
{
    bool enabled = false;
    float flipPicBlend = 0.95f;
    float apicAffineStrength = 1.0f;
    bool reuseVolumetricPressureProjection = true;
};

struct GpuProductionLiquidLodSettings
{
    bool enabled = true;
    float highQualityRadius = 10.0f;
    float simulationRadius = 24.0f;
    uint32_t developmentParticleCount = 1000;
    uint32_t interactiveParticleCount = 4096;
    uint32_t highParticleCount = 16384;
    uint32_t cinematicParticleCount = 32768;
    uint32_t stressParticleCount = 65536;
};

struct GpuProductionLiquidRuntimeStats
{
    uint64_t frameCount = 0;
    uint64_t estimatedProjectionDispatches = 0;
    uint64_t estimatedProjectionDispatchesSaved = 0;
    uint32_t selectedDensityIterations = 5;
    uint32_t selectedDivergenceIterations = 3;
    uint32_t calmFrameCount = 0;
    uint32_t activeParticleCount = 0;
    uint32_t surfaceSmoothingIterations = 2;
    uint32_t sprayCandidateCount = 0;
    uint32_t foamCandidateCount = 0;
    uint32_t bubbleCandidateCount = 0;
    float densityErrorRatio = 0.0f;
    float divergenceErrorRatio = 0.0f;
    float focusDistanceToSimulation = 0.0f;
    bool initialized = false;
    bool adaptiveSolverActive = false;
    bool flipApicFoundationAvailable = false;
    bool oceanProviderConnected = false;
    bool oceanSampleValid = false;
    bool oceanCouplingSucceeded = true;
    bool secondaryClassificationSucceeded = true;
    bool localSimulationVisible = true;
};

/// W10: DFSPH品質制御、Secondary、Spatial LOD、Ocean双方向Bridgeを束ねるProduction Liquid Controller。
class GpuProductionLiquidManager final
{
public:
    static GpuProductionLiquidManager* GetInstance()
    {
        static GpuProductionLiquidManager instance;
        return &instance;
    }

    bool Initialize()
    {
        settings_ = {};
        secondarySettings_ = {};
        flipApicSettings_ = {};
        oceanSettings_ = {};
        lodSettings_ = {};
        stats_ = {};
        focusPosition_ = {};
        preset_ = GpuProductionLiquidQualityPreset::High;
        initialized_ = true;
        ApplyQualityPreset(preset_, false);
        stats_.initialized = true;
        return true;
    }

    void Finalize()
    {
        GpuProductionLiquidOceanCoupler::GetInstance()->Finalize();
        GpuProductionLiquidSecondaryClassifier::GetInstance()->Finalize();
        oceanProvider_ = nullptr;
        settings_ = {};
        secondarySettings_ = {};
        flipApicSettings_ = {};
        oceanSettings_ = {};
        lodSettings_ = {};
        stats_ = {};
        initialized_ = false;
    }

    void SetSimulationFocus(const Vector3& worldPosition)
    {
        focusPosition_ = worldPosition;
    }

    void PreSphUpdate(float deltaTime)
    {
        if (!initialized_)
        {
            return;
        }

        GpuSphManager* sph = GpuSphManager::GetInstance();
        if (!sph || !sph->IsInitialized())
        {
            return;
        }

        GpuSphSimulationSettings& sphSettings = sph->GetEditableSimulationSettings();
        const GpuSphRuntimeStats& sphStats = sph->GetRuntimeStats();
        stats_.activeParticleCount = sphSettings.activeParticleCount;
        stats_.adaptiveSolverActive = settings_.enabled && sphSettings.dfsphEnabled;

        ApplySpatialLod(*sph, sphSettings);

        lastOceanSample_ = SampleOcean(focusPosition_);
        stats_.oceanSampleValid = lastOceanSample_.valid;
        if (lastOceanSample_.valid)
        {
            stats_.oceanCouplingSucceeded = GpuProductionLiquidOceanCoupler::GetInstance()->Update(
                sph->GetParticleBuffer(),
                deltaTime,
                lastOceanSample_,
                oceanSettings_.blendBand,
                oceanSettings_.velocityCoupling,
                oceanSettings_.surfaceAttraction,
                oceanSettings_.maxVelocityCorrection); // W10 Ocean→SPHはCamera近傍の実Gerstner SampleをPrimary粒子へ直接渡す。
        }

        if (!stats_.adaptiveSolverActive)
        {
            stats_.selectedDensityIterations = sphSettings.dfsphDensityIterations;
            stats_.selectedDivergenceIterations = sphSettings.dfsphDivergenceIterations;
            return;
        }

        const float densityTolerance = (std::max)(sphSettings.dfsphDensityErrorTolerance, 1.0e-5f);
        const float divergenceTolerance = (std::max)(sphSettings.dfsphDivergenceErrorTolerance, 1.0e-5f);
        stats_.densityErrorRatio = sphStats.lastMaxDensityError / densityTolerance;
        stats_.divergenceErrorRatio = sphStats.lastMaxDivergenceError / divergenceTolerance;

        if (sphStats.cflReadbackCount == 0)
        {
            stats_.selectedDensityIterations = sphSettings.dfsphDensityIterations;
            stats_.selectedDivergenceIterations = sphSettings.dfsphDivergenceIterations;
            return;
        }

        const uint32_t targetDensity = SelectIterationBudget(
            stats_.densityErrorRatio,
            settings_.minDensityIterations,
            settings_.maxDensityIterations);
        const uint32_t targetDivergence = SelectIterationBudget(
            stats_.divergenceErrorRatio,
            settings_.minDivergenceIterations,
            settings_.maxDivergenceIterations);

        const bool wantsDownshift =
            targetDensity < sphSettings.dfsphDensityIterations ||
            targetDivergence < sphSettings.dfsphDivergenceIterations;
        if (wantsDownshift)
        {
            ++stats_.calmFrameCount;
        }
        else
        {
            stats_.calmFrameCount = 0;
        }

        const bool allowDownshift = stats_.calmFrameCount >= (std::max)(settings_.calmFramesBeforeDownshift, 1u);
        if (targetDensity > sphSettings.dfsphDensityIterations || allowDownshift)
        {
            sphSettings.dfsphDensityIterations = targetDensity;
        }
        if (targetDivergence > sphSettings.dfsphDivergenceIterations || allowDownshift)
        {
            sphSettings.dfsphDivergenceIterations = targetDivergence;
        }
        if (allowDownshift)
        {
            stats_.calmFrameCount = 0;
        }

        stats_.selectedDensityIterations = sphSettings.dfsphDensityIterations;
        stats_.selectedDivergenceIterations = sphSettings.dfsphDivergenceIterations;
    }

    void PostSphUpdate()
    {
        if (!initialized_)
        {
            return;
        }

        GpuSphManager* sph = GpuSphManager::GetInstance();
        if (!sph || !sph->IsInitialized())
        {
            return;
        }

        const GpuSphRuntimeStats& sphStats = sph->GetRuntimeStats();
        const uint64_t substeps = sphStats.lastFrameSubsteps;
        const uint64_t baselinePerStep = 2ull *
            (static_cast<uint64_t>(settings_.maxDensityIterations) + static_cast<uint64_t>(settings_.maxDivergenceIterations));
        const uint64_t selectedPerStep = 2ull *
            (static_cast<uint64_t>(stats_.selectedDensityIterations) + static_cast<uint64_t>(stats_.selectedDivergenceIterations));
        stats_.estimatedProjectionDispatches += selectedPerStep * substeps;
        if (baselinePerStep > selectedPerStep)
        {
            stats_.estimatedProjectionDispatchesSaved += (baselinePerStep - selectedPerStep) * substeps;
        }

        GpuSphSimulationSettings& sphSettings = sph->GetEditableSimulationSettings();
        GpuProductionLiquidSecondaryClassifier* classifier = GpuProductionLiquidSecondaryClassifier::GetInstance();
        stats_.secondaryClassificationSucceeded = classifier->Update(
            sph->GetParticleBuffer(),
            secondarySettings_.enabled,
            secondarySettings_.classificationIntervalFrames,
            sphSettings.targetDensity,
            secondarySettings_.spraySpeedThreshold,
            secondarySettings_.foamSpeedThreshold,
            secondarySettings_.freeSurfaceDensityRatio,
            secondarySettings_.bubbleDensityRatio); // Secondary分類はSPH終了後に同じParticle Bufferを読み取り、Primary Solverを汚さない。

        const GpuProductionLiquidSecondaryClassifierStats& secondaryStats = classifier->GetStats();
        stats_.sprayCandidateCount = secondaryStats.sprayCandidateCount;
        stats_.foamCandidateCount = secondaryStats.foamCandidateCount;
        stats_.bubbleCandidateCount = secondaryStats.bubbleCandidateCount;

        if (oceanSettings_.enabled && oceanProvider_ && lastOceanSample_.valid && sphSettings.activeParticleCount > 0)
        {
            const float secondaryRatio = static_cast<float>(
                stats_.sprayCandidateCount + stats_.foamCandidateCount) /
                static_cast<float>((std::max)(sphSettings.activeParticleCount, 1u));
            const float feedbackMagnitude = oceanSettings_.feedbackStrength * (std::min)(secondaryRatio, 1.0f);
            const Vector3 impulse = lastOceanSample_.normal * feedbackMagnitude;
            oceanProvider_->SubmitLiquidFeedback(focusPosition_, impulse, oceanSettings_.feedbackRadius); // SPH側の自由表面活動量をOceanへ戻し、双方向Bridgeを閉じる。
        }

        stats_.flipApicFoundationAvailable = GpuVolumetricFluidManager::GetInstance()->IsInitialized();
        stats_.oceanProviderConnected = oceanProvider_ != nullptr;
        ++stats_.frameCount;
    }

    void ApplyQualityPreset(GpuProductionLiquidQualityPreset preset, bool applyParticleCount)
    {
        preset_ = preset;
        GpuSphManager* sph = GpuSphManager::GetInstance();
        GpuSphScreenSpaceFluidRenderer* renderer = GpuSphScreenSpaceFluidRenderer::GetInstance();
        uint32_t particleCount = 0;
        uint32_t smoothingBudget = 2;
        float blurDepthFalloff = 28.0f;

        switch (preset)
        {
        case GpuProductionLiquidQualityPreset::Development:
            settings_.minDensityIterations = 1;
            settings_.maxDensityIterations = 4;
            settings_.minDivergenceIterations = 0;
            settings_.maxDivergenceIterations = 2;
            secondarySettings_.classificationIntervalFrames = 4;
            particleCount = lodSettings_.developmentParticleCount;
            smoothingBudget = 1;
            blurDepthFalloff = 18.0f;
            break;
        case GpuProductionLiquidQualityPreset::Interactive:
            settings_.minDensityIterations = 2;
            settings_.maxDensityIterations = 5;
            settings_.minDivergenceIterations = 1;
            settings_.maxDivergenceIterations = 3;
            secondarySettings_.classificationIntervalFrames = 2;
            particleCount = lodSettings_.interactiveParticleCount;
            smoothingBudget = 2;
            blurDepthFalloff = 24.0f;
            break;
        case GpuProductionLiquidQualityPreset::High:
            settings_.minDensityIterations = 2;
            settings_.maxDensityIterations = 8;
            settings_.minDivergenceIterations = 1;
            settings_.maxDivergenceIterations = 5;
            secondarySettings_.classificationIntervalFrames = 2;
            particleCount = lodSettings_.highParticleCount;
            smoothingBudget = 3;
            blurDepthFalloff = 28.0f;
            break;
        case GpuProductionLiquidQualityPreset::Cinematic:
            settings_.minDensityIterations = 3;
            settings_.maxDensityIterations = 10;
            settings_.minDivergenceIterations = 2;
            settings_.maxDivergenceIterations = 7;
            secondarySettings_.classificationIntervalFrames = 1;
            particleCount = lodSettings_.cinematicParticleCount;
            smoothingBudget = 4;
            blurDepthFalloff = 34.0f;
            break;
        case GpuProductionLiquidQualityPreset::Stress:
        default:
            settings_.minDensityIterations = 2;
            settings_.maxDensityIterations = 6;
            settings_.minDivergenceIterations = 1;
            settings_.maxDivergenceIterations = 4;
            secondarySettings_.classificationIntervalFrames = 4;
            particleCount = lodSettings_.stressParticleCount;
            smoothingBudget = 1;
            blurDepthFalloff = 18.0f;
            break;
        }

        renderer->GetEditableSettings().blurDepthFalloff = blurDepthFalloff;
        stats_.surfaceSmoothingIterations = smoothingBudget;
        if (applyParticleCount && sph && sph->IsInitialized())
        {
            sph->SetActiveParticleCount(particleCount);
        }
    }

    void SetOceanProvider(IGpuProductionLiquidOceanProvider* provider)
    {
        oceanProvider_ = provider;
        stats_.oceanProviderConnected = provider != nullptr;
    }

    void ClearOceanProvider(const IGpuProductionLiquidOceanProvider* provider)
    {
        if (oceanProvider_ == provider)
        {
            SetOceanProvider(nullptr); // 複数WaterSurfaceがある場合に別SurfaceのProviderを誤って解除しない。
        }
    }

    [[nodiscard]] bool HasOceanProvider() const { return oceanProvider_ != nullptr; }

    [[nodiscard]] GpuProductionLiquidOceanSample SampleOcean(const Vector3& worldPosition) const
    {
        if (!oceanSettings_.enabled || oceanProvider_ == nullptr)
        {
            return {};
        }
        return oceanProvider_->SampleOcean(worldPosition);
    }

    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] GpuProductionLiquidAdaptiveSolverSettings& GetEditableAdaptiveSolverSettings() { return settings_; }
    [[nodiscard]] GpuProductionLiquidSecondarySettings& GetEditableSecondarySettings() { return secondarySettings_; }
    [[nodiscard]] GpuProductionLiquidFlipApicSettings& GetEditableFlipApicSettings() { return flipApicSettings_; }
    [[nodiscard]] GpuProductionLiquidOceanBridgeSettings& GetEditableOceanSettings() { return oceanSettings_; }
    [[nodiscard]] GpuProductionLiquidLodSettings& GetEditableLodSettings() { return lodSettings_; }
    [[nodiscard]] const GpuProductionLiquidRuntimeStats& GetRuntimeStats() const { return stats_; }
    [[nodiscard]] GpuProductionLiquidQualityPreset GetQualityPreset() const { return preset_; }

private:
    static uint32_t SelectIterationBudget(float ratio, uint32_t minimum, uint32_t maximum)
    {
        minimum = (std::min)(minimum, maximum);
        if (ratio <= 0.75f) return minimum;
        if (ratio <= 1.5f) return (std::min)(minimum + 1u, maximum);
        if (ratio <= 3.0f) return minimum + (maximum - minimum) / 2u;
        return maximum;
    }

    void ApplySpatialLod(GpuSphManager& sph, const GpuSphSimulationSettings& sphSettings)
    {
        if (!lodSettings_.enabled)
        {
            stats_.localSimulationVisible = true;
            return;
        }

        const Vector3 center = (sphSettings.boundaryMin + sphSettings.boundaryMax) * 0.5f;
        const float dx = focusPosition_.x - center.x;
        const float dz = focusPosition_.z - center.z;
        stats_.focusDistanceToSimulation = std::sqrt(dx * dx + dz * dz);

        GpuSphScreenSpaceRenderSettings& renderSettings = GpuSphScreenSpaceFluidRenderer::GetInstance()->GetEditableSettings();
        const bool visible = stats_.focusDistanceToSimulation <= (std::max)(lodSettings_.simulationRadius, 0.1f);
        renderSettings.enabled = visible;
        stats_.localSimulationVisible = visible;

        uint32_t targetCount = lodSettings_.developmentParticleCount;
        if (stats_.focusDistanceToSimulation <= lodSettings_.highQualityRadius)
        {
            targetCount = lodSettings_.highParticleCount;
        }
        else if (visible)
        {
            targetCount = lodSettings_.interactiveParticleCount;
        }
        targetCount = (std::max)(1u, targetCount);
        if (sphSettings.activeParticleCount != targetCount)
        {
            sph.SetActiveParticleCount(targetCount); // LOD帯を跨いだ時だけ再配置し、毎FrameのParticle Resetを避ける。
        }
    }

    GpuProductionLiquidManager() = default;

    GpuProductionLiquidAdaptiveSolverSettings settings_{};
    GpuProductionLiquidSecondarySettings secondarySettings_{};
    GpuProductionLiquidFlipApicSettings flipApicSettings_{};
    GpuProductionLiquidOceanBridgeSettings oceanSettings_{};
    GpuProductionLiquidLodSettings lodSettings_{};
    GpuProductionLiquidRuntimeStats stats_{};
    GpuProductionLiquidQualityPreset preset_ = GpuProductionLiquidQualityPreset::High;
    IGpuProductionLiquidOceanProvider* oceanProvider_ = nullptr;
    GpuProductionLiquidOceanSample lastOceanSample_{};
    Vector3 focusPosition_{};
    bool initialized_ = false;
};

} // namespace Ken4lowEngine
