#pragma once

#include "../Resource/GpuSphParticleBuffer.h"

#include <DX12Include.h>
#include <Vector3.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class DirectXCommon;
enum class GpuSphComputeShaderId : uint32_t;

struct GpuSphSimulationSettings
{
    float fixedDeltaTime = 1.0f / 120.0f;
    uint32_t maxSubsteps = 4;
    uint32_t activeParticleCount = 1000;
    float particleMass = 0.729f;
    float smoothingRadius = 0.18f;
    float targetDensity = 1000.0f;
    float pressureStiffness = 120.0f;
    float viscosityStrength = 0.08f;
    float boundaryDamping = 0.05f;
    Vector3 gravity{ 0.0f, -9.81f, 0.0f };
    Vector3 boundaryMin{ -2.0f, 0.0f, -2.0f };
    Vector3 boundaryMax{ 2.0f, 4.0f, 2.0f };
    Vector3 spawnOrigin{ -0.675f, 0.25f, -0.675f };
    float spawnSpacing = 0.09f;
    uint32_t spawnDimX = 16;
    uint32_t spawnDimY = 16;
    uint32_t spawnDimZ = 16;

    // W9.5: WCSPHをFallbackとして残しながらDFSPH projectionを有効化する。
    bool dfsphEnabled = true;
    uint32_t dfsphDensityIterations = 5;
    uint32_t dfsphDivergenceIterations = 3;
    bool adaptiveCflEnabled = true;
    float dfsphDensityRelaxation = 0.45f;
    float dfsphDivergenceRelaxation = 0.35f;
    float dfsphDensityErrorTolerance = 0.01f;
    float dfsphDivergenceErrorTolerance = 0.01f;
    float cflNumber = 0.35f;
    float minimumDeltaTime = 1.0f / 480.0f;
    float surfaceTensionStrength = 0.0728f;
    float xsphStrength = 0.025f;
    float boundaryFriction = 0.08f;
    float maxDfsphVelocityCorrection = 2.0f;
    bool dfsphWarmStartEnabled = true;
    float dfsphWarmStartStrength = 0.35f;

    // W10: Ocean側の局所波面をPrimary SPHへ流速・表面拘束として与える。
    bool oceanCouplingEnabled = false;
    float oceanVelocityCoupling = 3.0f;
    float oceanSurfaceAttraction = 6.0f;
    float oceanBlendBand = 3.0f;
    Vector3 oceanSurfacePoint{};
    float oceanMaxCorrection = 4.0f;
    Vector3 oceanSurfaceNormal{ 0.0f, 1.0f, 0.0f };
    float oceanPadding0 = 0.0f;
    Vector3 oceanSurfaceVelocity{};
    float oceanPadding1 = 0.0f;
};

struct GpuSphRuntimeStats
{
    uint64_t totalSimulationSteps = 0;
    uint64_t resetCount = 0;
    uint64_t totalDispatchCount = 0;
    uint64_t gravityDispatchCount = 0;
    uint64_t boundaryDispatchCount = 0;
    uint64_t densityDispatchCount = 0;
    uint64_t pressureDispatchCount = 0;
    uint64_t viscosityDispatchCount = 0;
    uint64_t predictionDispatchCount = 0;
    uint64_t spatialHashBuildCount = 0;
    uint64_t spatialHashSortDispatchCount = 0;
    uint64_t cellRangeDispatchCount = 0;
    uint64_t dfsphFactorDispatchCount = 0;
    uint64_t dfsphDensityDispatchCount = 0;
    uint64_t dfsphDivergenceDispatchCount = 0;
    uint64_t cflMetricDispatchCount = 0;
    uint64_t cflReadbackCount = 0;
    uint64_t cflStabilizationCount = 0;
    uint32_t lastFrameSubsteps = 0;
    uint32_t sortedParticleCount = 0;
    uint32_t spatialGridDimX = 0;
    uint32_t spatialGridDimY = 0;
    uint32_t spatialGridDimZ = 0;
    uint32_t spatialCellCount = 0;
    uint32_t lastDensityIterations = 0;
    uint32_t lastDivergenceIterations = 0;
    uint32_t frameResourceCount = 0;
    uint64_t approximateGpuMemoryBytes = 0;
    float spatialCellSize = 0.0f;
    float accumulatorSeconds = 0.0f;
    float effectiveDeltaTime = 0.0f;
    float lastMeasuredMaxSpeed = 0.0f;
    float lastMaxDensityError = 0.0f;
    float lastMaxDivergenceError = 0.0f;
    bool initialized = false;
    bool paused = false;
    bool lastStepSucceeded = true;
    bool spatialHashReady = false;
    bool dfsphActive = false;
};

/// W5-W10のGPU SPH / Spatial Hash / DFSPH / Ocean Couplingを所有するRuntime。
class GpuSphManager
{
public:
    static constexpr uint32_t kDefaultParticleCapacity = 65536;
    static constexpr uint32_t kDefaultActiveParticleCount = 1000;
    static constexpr uint32_t kMaxSpatialCellCapacity = 1u << 20;

    static GpuSphManager* GetInstance();

    bool Initialize(uint32_t particleCapacity = kDefaultParticleCapacity);
    void Finalize();
    void Update(float deltaTime);

    void RequestReset() { resetRequested_ = true; }
    void RequestSingleStep() { singleStepRequested_ = true; }
    void SetPaused(bool paused) { paused_ = paused; }
    [[nodiscard]] bool IsPaused() const { return paused_; }
    void SetActiveParticleCount(uint32_t activeCount);
    void ApplyWaterProductionPreset();

    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] GpuSphParticleBuffer& GetParticleBuffer() { return particleBuffer_; }
    [[nodiscard]] const GpuSphParticleBuffer& GetParticleBuffer() const { return particleBuffer_; }
    [[nodiscard]] GpuSphParticleBufferStats GetParticleBufferStats() const { return particleBuffer_.GetStats(); }
    [[nodiscard]] const GpuSphSimulationSettings& GetSimulationSettings() const { return settings_; }
    [[nodiscard]] GpuSphSimulationSettings& GetEditableSimulationSettings() { return settings_; }
    [[nodiscard]] const GpuSphRuntimeStats& GetRuntimeStats() const { return stats_; }

private:
    struct GpuSphSimulationConstants
    {
        uint32_t activeParticleCount = 0;
        float deltaTime = 0.0f;
        float particleMass = 0.0f;
        float smoothingRadius = 0.0f;

        float targetDensity = 0.0f;
        float pressureStiffness = 0.0f;
        float viscosityStrength = 0.0f;
        float boundaryDamping = 0.0f;

        Vector3 gravity{};
        float padding0 = 0.0f;
        Vector3 boundaryMin{};
        float padding1 = 0.0f;
        Vector3 boundaryMax{};
        float padding2 = 0.0f;
        Vector3 spawnOrigin{};
        float spawnSpacing = 0.0f;

        uint32_t spawnDimX = 0;
        uint32_t spawnDimY = 0;
        uint32_t spawnDimZ = 0;
        uint32_t padding3 = 0;

        Vector3 spatialGridMin{};
        float spatialCellSize = 0.0f;
        uint32_t spatialGridDimX = 0;
        uint32_t spatialGridDimY = 0;
        uint32_t spatialGridDimZ = 0;
        uint32_t spatialCellCount = 0;

        uint32_t dfsphEnabled = 0;
        uint32_t dfsphDensityIterations = 0;
        uint32_t dfsphDivergenceIterations = 0;
        uint32_t adaptiveCflEnabled = 0;

        float dfsphDensityRelaxation = 0.0f;
        float dfsphDivergenceRelaxation = 0.0f;
        float dfsphDensityErrorTolerance = 0.0f;
        float dfsphDivergenceErrorTolerance = 0.0f;

        float cflNumber = 0.0f;
        float minimumDeltaTime = 0.0f;
        float surfaceTensionStrength = 0.0f;
        float xsphStrength = 0.0f;

        float boundaryFriction = 0.0f;
        float maxDfsphVelocityCorrection = 0.0f;
        uint32_t dfsphWarmStartEnabled = 0;
        float dfsphWarmStartStrength = 0.0f;

        uint32_t oceanCouplingEnabled = 0;
        float oceanVelocityCoupling = 0.0f;
        float oceanSurfaceAttraction = 0.0f;
        float oceanBlendBand = 0.0f;
        Vector3 oceanSurfacePoint{};
        float oceanMaxCorrection = 0.0f;
        Vector3 oceanSurfaceNormal{ 0.0f, 1.0f, 0.0f };
        float oceanPadding0 = 0.0f;
        Vector3 oceanSurfaceVelocity{};
        float oceanPadding1 = 0.0f;
    };
    static_assert(sizeof(GpuSphSimulationConstants) == 272);

    struct GpuSphDispatchConstants
    {
        uint32_t sortLevel = 0;
        uint32_t sortLevelMask = 0;
        uint32_t sortCount = 0;
        uint32_t cellCount = 0;
    };
    static_assert(sizeof(GpuSphDispatchConstants) == 16);

    struct CflMetricReadback
    {
        uint32_t maxSpeed = 0;
        uint32_t maxDensityError = 0;
        uint32_t maxDivergenceError = 0;
        uint32_t padding = 0;
    };
    static_assert(sizeof(CflMetricReadback) == 16);

    struct CflReadbackSlot
    {
        ComPtr<ID3D12Resource> buffer{};
        bool pending = false;
    };

    static constexpr uint32_t kThreadGroupSize = 128;
    static constexpr std::size_t kPipelineStateCount = 22;
    static constexpr float kCflMetricScale = 1000.0f;
    static constexpr float kConstraintMetricScale = 1000000.0f;

    GpuSphManager() = default;
    ~GpuSphManager() = default;
    GpuSphManager(const GpuSphManager&) = delete;
    GpuSphManager& operator=(const GpuSphManager&) = delete;

    bool CreateRootSignature();
    bool CreatePipelineStates();
    bool CreatePipelineState(GpuSphComputeShaderId shaderId, ComPtr<ID3D12PipelineState>& pipelineState);
    bool CreateScratchBuffer(uint32_t capacity);
    void ReleaseScratchBuffer();
    bool CreateDfSphStateBuffer(uint32_t capacity);
    void ReleaseDfSphStateBuffer();
    bool CreateSpatialHashBuffers(uint32_t particleCapacity);
    void ReleaseSpatialHashBuffers();
    bool CreateCflReadbackBuffers();
    void ReleaseCflReadbackBuffers();
    void ConsumeCflReadback();
    bool ScheduleCflReadback();
    bool ExecuteReset();
    bool ExecuteSimulationStep(float deltaTime);
    bool ExecuteDfSphProjection(D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress, uint32_t activeCount);
    bool ExecuteSpatialHashBuild(D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
    bool DispatchStage(
        GpuSphComputeShaderId shaderId,
        D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
        uint32_t dispatchItemCount,
        const GpuSphDispatchConstants& dispatchConstants,
        bool particleBarrier,
        bool scratchBarrier,
        bool hashBarrier,
        bool cellRangeBarrier,
        bool dfsphStateBarrier = false);
    [[nodiscard]] GpuSphSimulationConstants BuildConstants(float deltaTime) const;
    [[nodiscard]] uint32_t GetValidatedActiveParticleCount() const;
    [[nodiscard]] uint32_t GetSortCount(uint32_t activeCount) const;
    [[nodiscard]] float CalculateEffectiveDeltaTime(float requestedDeltaTime) const;
    void UpdateSpawnLayoutForActiveCount(uint32_t activeCount);
    void InsertUavBarrier(ID3D12Resource* resource) const;
    void TransitionHashBuffer(
        ID3D12GraphicsCommandList* commandList,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState) const;
    void RefreshStats(uint32_t substeps, bool lastStepSucceeded);

private:
    DirectXCommon* dxCommon_ = nullptr;
    GpuSphParticleBuffer particleBuffer_{};
    ComPtr<ID3D12Resource> scratchBuffer_{};
    ComPtr<ID3D12Resource> dfsphStateBuffer_{};
    ComPtr<ID3D12Resource> hashEntriesBuffer_{};
    ComPtr<ID3D12Resource> cellRangesBuffer_{};
    uint32_t scratchUavIndex_ = UINT32_MAX;
    uint32_t dfsphStateUavIndex_ = UINT32_MAX;
    uint32_t hashEntriesUavIndex_ = UINT32_MAX;
    uint32_t cellRangesUavIndex_ = UINT32_MAX;
    uint32_t hashEntryCapacity_ = 0;
    uint32_t cellRangeCapacity_ = 0;
    ComPtr<ID3D12RootSignature> rootSignature_{};
    std::array<ComPtr<ID3D12PipelineState>, kPipelineStateCount> pipelineStates_{};
    std::vector<CflReadbackSlot> cflReadbackSlots_{};
    GpuSphSimulationSettings settings_{};
    GpuSphRuntimeStats stats_{};
    float accumulatorSeconds_ = 0.0f;
    float effectiveDeltaTime_ = 1.0f / 120.0f;
    float lastMeasuredMaxSpeed_ = 0.0f;
    float lastMaxDensityError_ = 0.0f;
    float lastMaxDivergenceError_ = 0.0f;
    bool initialized_ = false;
    bool paused_ = false;
    bool resetRequested_ = false;
    bool singleStepRequested_ = false;
};

} // namespace Ken4lowEngine
