#pragma once

#include "../Resource/GpuSphParticleBuffer.h"

#include <DX12Include.h>
#include <Vector3.h>

#include <array>
#include <cstddef>
#include <cstdint>

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
    uint32_t lastFrameSubsteps = 0;
    uint32_t sortedParticleCount = 0;
    uint32_t spatialGridDimX = 0;
    uint32_t spatialGridDimY = 0;
    uint32_t spatialGridDimZ = 0;
    uint32_t spatialCellCount = 0;
    uint64_t approximateGpuMemoryBytes = 0;
    float spatialCellSize = 0.0f;
    float accumulatorSeconds = 0.0f;
    bool initialized = false;
    bool paused = false;
    bool lastStepSucceeded = true;
    bool spatialHashReady = false;
};

/// W5 SPH計算とW6 Spatial Hash / GPU Sortを所有するRuntime。
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
    };
    static_assert(sizeof(GpuSphSimulationConstants) == 144);

    struct GpuSphDispatchConstants
    {
        uint32_t sortLevel = 0;
        uint32_t sortLevelMask = 0;
        uint32_t sortCount = 0;
        uint32_t cellCount = 0;
    };
    static_assert(sizeof(GpuSphDispatchConstants) == 16);

    static constexpr uint32_t kThreadGroupSize = 128;
    static constexpr std::size_t kPipelineStateCount = 15;

    GpuSphManager() = default;
    ~GpuSphManager() = default;
    GpuSphManager(const GpuSphManager&) = delete;
    GpuSphManager& operator=(const GpuSphManager&) = delete;

    bool CreateRootSignature();
    bool CreatePipelineStates();
    bool CreatePipelineState(GpuSphComputeShaderId shaderId, ComPtr<ID3D12PipelineState>& pipelineState);
    bool CreateScratchBuffer(uint32_t capacity);
    void ReleaseScratchBuffer();
    bool CreateSpatialHashBuffers(uint32_t particleCapacity);
    void ReleaseSpatialHashBuffers();
    bool ExecuteReset();
    bool ExecuteSimulationStep(float deltaTime);
    bool ExecuteSpatialHashBuild(D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress);
    bool DispatchStage(
        GpuSphComputeShaderId shaderId,
        D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
        uint32_t dispatchItemCount,
        const GpuSphDispatchConstants& dispatchConstants,
        bool particleBarrier,
        bool scratchBarrier,
        bool hashBarrier,
        bool cellRangeBarrier);
    [[nodiscard]] GpuSphSimulationConstants BuildConstants(float deltaTime) const;
    [[nodiscard]] uint32_t GetValidatedActiveParticleCount() const;
    [[nodiscard]] uint32_t GetSortCount(uint32_t activeCount) const;
    void UpdateSpawnLayoutForActiveCount(uint32_t activeCount);
    void InsertUavBarrier(ID3D12Resource* resource) const;
    void RefreshStats(uint32_t substeps, bool lastStepSucceeded);

private:
    DirectXCommon* dxCommon_ = nullptr;
    GpuSphParticleBuffer particleBuffer_{};
    ComPtr<ID3D12Resource> scratchBuffer_{};
    ComPtr<ID3D12Resource> hashEntriesBuffer_{};
    ComPtr<ID3D12Resource> cellRangesBuffer_{};
    uint32_t scratchUavIndex_ = UINT32_MAX;
    uint32_t hashEntriesUavIndex_ = UINT32_MAX;
    uint32_t cellRangesUavIndex_ = UINT32_MAX;
    uint32_t hashEntryCapacity_ = 0;
    uint32_t cellRangeCapacity_ = 0;
    ComPtr<ID3D12RootSignature> rootSignature_{};
    std::array<ComPtr<ID3D12PipelineState>, kPipelineStateCount> pipelineStates_{};
    GpuSphSimulationSettings settings_{};
    GpuSphRuntimeStats stats_{};
    float accumulatorSeconds_ = 0.0f;
    bool initialized_ = false;
    bool paused_ = false;
    bool resetRequested_ = false;
    bool singleStepRequested_ = false;
};

} // namespace Ken4lowEngine
