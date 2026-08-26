#include "GpuSphManager.h"

#include <DirectXCommon.h>
#include <GpuSphShaderManifest.h>
#include <LogString.h>
#include <ResourceManager.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace Ken4lowEngine
{

namespace
{
struct SpatialGridDesc
{
    Vector3 min{};
    float cellSize = 0.0f;
    uint32_t dimX = 0;
    uint32_t dimY = 0;
    uint32_t dimZ = 0;
    uint32_t cellCount = 0;
    bool valid = false;
};

SpatialGridDesc BuildSpatialGridDesc(
    const GpuSphSimulationSettings& settings,
    uint32_t maxCellCapacity)
{
    SpatialGridDesc desc{};
    desc.min = settings.boundaryMin;
    desc.cellSize = (std::max)(settings.smoothingRadius, 0.001f);

    const float extentX = settings.boundaryMax.x - settings.boundaryMin.x;
    const float extentY = settings.boundaryMax.y - settings.boundaryMin.y;
    const float extentZ = settings.boundaryMax.z - settings.boundaryMin.z;
    if (extentX <= 0.0f || extentY <= 0.0f || extentZ <= 0.0f)
    {
        return desc;
    }

    desc.dimX = (std::max)(1u, static_cast<uint32_t>(std::ceil(extentX / desc.cellSize)));
    desc.dimY = (std::max)(1u, static_cast<uint32_t>(std::ceil(extentY / desc.cellSize)));
    desc.dimZ = (std::max)(1u, static_cast<uint32_t>(std::ceil(extentZ / desc.cellSize)));

    const uint64_t cellCount =
        static_cast<uint64_t>(desc.dimX) *
        static_cast<uint64_t>(desc.dimY) *
        static_cast<uint64_t>(desc.dimZ);
    if (cellCount == 0 || cellCount > maxCellCapacity)
    {
        return desc;
    }

    desc.cellCount = static_cast<uint32_t>(cellCount);
    desc.valid = true;
    return desc;
}
}

GpuSphManager* GpuSphManager::GetInstance()
{
    static GpuSphManager instance;
    return &instance;
}

bool GpuSphManager::Initialize(uint32_t particleCapacity)
{
    Finalize();

    dxCommon_ = DirectXCommon::GetInstance();
    if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr || particleCapacity == 0)
    {
        return false;
    }

    settings_ = {};
    settings_.activeParticleCount = (std::min)(kDefaultActiveParticleCount, particleCapacity);
    effectiveDeltaTime_ = settings_.fixedDeltaTime;

    if (!particleBuffer_.Initialize(particleCapacity) ||
        !CreateScratchBuffer(particleCapacity) ||
        !CreateSpatialHashBuffers(particleCapacity) ||
        !CreateCflReadbackBuffers() ||
        !CreateRootSignature() ||
        !CreatePipelineStates())
    {
        Finalize();
        return false;
    }

    // W6では近傍探索をSpatial Hashへ切り替え、Buffer CapacityまでActive粒子を許可する。
    SetActiveParticleCount(settings_.activeParticleCount);
    initialized_ = true;
    resetRequested_ = true;
    RefreshStats(0, true);
    return true;
}

void GpuSphManager::Finalize()
{
    for (auto& pipelineState : pipelineStates_)
    {
        pipelineState.Reset();
    }
    rootSignature_.Reset();
    ReleaseCflReadbackBuffers();
    ReleaseSpatialHashBuffers();
    ReleaseScratchBuffer();
    particleBuffer_.Finalize();

    settings_ = {};
    stats_ = {};
    accumulatorSeconds_ = 0.0f;
    effectiveDeltaTime_ = 1.0f / 120.0f;
    lastMeasuredMaxSpeed_ = 0.0f;
    dxCommon_ = nullptr;
    initialized_ = false;
    paused_ = false;
    resetRequested_ = false;
    singleStepRequested_ = false;
}

void GpuSphManager::Update(float deltaTime)
{
    if (!initialized_ || deltaTime <= 0.0f)
    {
        return;
    }

    ConsumeCflReadback();

    bool stepSucceeded = true;
    if (resetRequested_)
    {
        stepSucceeded = ExecuteReset();
        resetRequested_ = false;
        if (!stepSucceeded)
        {
            RefreshStats(0, false);
            return;
        }
    }

    const float requestedFixedDeltaTime = (std::max)(settings_.fixedDeltaTime, 1.0f / 1000.0f);
    effectiveDeltaTime_ = CalculateEffectiveDeltaTime(requestedFixedDeltaTime);
    const float fixedDeltaTime = effectiveDeltaTime_;
    const uint32_t maxSubsteps = (std::clamp)(settings_.maxSubsteps, 1u, 16u);
    uint32_t substeps = 0;

    if (singleStepRequested_)
    {
        stepSucceeded = ExecuteSimulationStep(fixedDeltaTime);
        substeps = stepSucceeded ? 1u : 0u;
        singleStepRequested_ = false;
        accumulatorSeconds_ = 0.0f;
    }
    else if (!paused_)
    {
        accumulatorSeconds_ += (std::min)(deltaTime, 0.25f);
        while (accumulatorSeconds_ >= fixedDeltaTime && substeps < maxSubsteps)
        {
            if (!ExecuteSimulationStep(fixedDeltaTime))
            {
                stepSucceeded = false;
                break;
            }

            accumulatorSeconds_ -= fixedDeltaTime;
            ++substeps;
        }

        accumulatorSeconds_ = (std::min)(accumulatorSeconds_, fixedDeltaTime * static_cast<float>(maxSubsteps));
    }

    if (stepSucceeded && substeps > 0 && settings_.adaptiveCflEnabled)
    {
        ScheduleCflReadback();
    }

    RefreshStats(substeps, stepSucceeded);
}

void GpuSphManager::SetActiveParticleCount(uint32_t activeCount)
{
    settings_.activeParticleCount = activeCount;
    const uint32_t validatedCount = GetValidatedActiveParticleCount();
    settings_.activeParticleCount = validatedCount;
    particleBuffer_.SetActiveParticleCount(validatedCount);
    UpdateSpawnLayoutForActiveCount(validatedCount);
    resetRequested_ = true;
}

void GpuSphManager::ApplyWaterProductionPreset()
{
    settings_.dfsphEnabled = true;
    settings_.dfsphDensityIterations = 5;
    settings_.dfsphDivergenceIterations = 3;
    settings_.dfsphDensityRelaxation = 0.45f;
    settings_.dfsphDivergenceRelaxation = 0.35f;
    settings_.dfsphDensityErrorTolerance = 0.01f;
    settings_.dfsphDivergenceErrorTolerance = 0.01f;
    settings_.adaptiveCflEnabled = true;
    settings_.cflNumber = 0.35f;
    settings_.minimumDeltaTime = 1.0f / 480.0f;
    settings_.surfaceTensionStrength = 0.0728f;
    settings_.xsphStrength = 0.025f;
    settings_.boundaryDamping = 0.05f;
    settings_.boundaryFriction = 0.08f;
    settings_.maxDfsphVelocityCorrection = 2.0f;
    settings_.viscosityStrength = 0.08f;
}

bool GpuSphManager::CreateRootSignature()
{
    if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
    {
        return false;
    }

    D3D12_ROOT_PARAMETER rootParameters[6]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.RegisterSpace = 0;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE scratchUavRange{};
    scratchUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    scratchUavRange.NumDescriptors = 1;
    scratchUavRange.BaseShaderRegister = 1;
    scratchUavRange.RegisterSpace = 0;
    scratchUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &scratchUavRange;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE hashUavRange{};
    hashUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    hashUavRange.NumDescriptors = 1;
    hashUavRange.BaseShaderRegister = 2;
    hashUavRange.RegisterSpace = 0;
    hashUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &hashUavRange;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE cellRangeUavRange{};
    cellRangeUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    cellRangeUavRange.NumDescriptors = 1;
    cellRangeUavRange.BaseShaderRegister = 3;
    cellRangeUavRange.RegisterSpace = 0;
    cellRangeUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &cellRangeUavRange;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[5].Constants.ShaderRegister = 1;
    rootParameters[5].Constants.RegisterSpace = 0;
    rootParameters[5].Constants.Num32BitValues = 4;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = _countof(rootParameters);
    desc.pParameters = rootParameters;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    if (FAILED(D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob)))
    {
        if (errorBlob)
        {
            Log(std::string(
                static_cast<const char*>(errorBlob->GetBufferPointer()),
                errorBlob->GetBufferSize()));
        }
        return false;
    }

    if (FAILED(dxCommon_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_))))
    {
        return false;
    }

    rootSignature_->SetName(L"GpuSph.W9.5.RootSignature");
    return true;
}

bool GpuSphManager::CreatePipelineStates()
{
    static_assert(static_cast<std::size_t>(GpuSphComputeShaderId::Count) == kPipelineStateCount);

    for (std::size_t index = 0; index < kPipelineStateCount; ++index)
    {
        if (!CreatePipelineState(
            static_cast<GpuSphComputeShaderId>(index),
            pipelineStates_[index]))
        {
            return false;
        }
    }
    return true;
}

bool GpuSphManager::CreatePipelineState(
    GpuSphComputeShaderId shaderId,
    ComPtr<ID3D12PipelineState>& pipelineState)
{
    const ShaderDescriptor& shaderDesc = GpuSphShaderManifest::GetCompute(shaderId);
    if (shaderDesc.stage != ShaderStage::Compute || shaderDesc.rootSignature != RootSignatureType::Compute)
    {
        return false;
    }

    ComPtr<IDxcBlob> shader = ShaderCompiler::CompileShader(shaderDesc, dxCommon_->GetDXCCompilerManager());
    if (!shader)
    {
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
    return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState)));
}

bool GpuSphManager::CreateScratchBuffer(uint32_t capacity)
{
    if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr || capacity == 0)
    {
        return false;
    }

    constexpr uint32_t kScratchStride = sizeof(float) * 4;
    scratchBuffer_ = ResourceManager::CreateBufferResource(
        dxCommon_->GetDevice(),
        static_cast<uint64_t>(capacity) * kScratchStride,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (!scratchBuffer_)
    {
        return false;
    }

    scratchBuffer_->SetName(L"GpuSph.W9.5.SharedSolverScratch");
    try
    {
        scratchUavIndex_ = UAVManager::GetInstance()->Allocate();
        UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
            scratchUavIndex_,
            scratchBuffer_.Get(),
            capacity,
            kScratchStride);
    }
    catch (...)
    {
        ReleaseScratchBuffer();
        return false;
    }

    return true;
}

void GpuSphManager::ReleaseScratchBuffer()
{
    if (scratchUavIndex_ != UINT32_MAX)
    {
        UAVManager::GetInstance()->Free(scratchUavIndex_);
    }
    scratchUavIndex_ = UINT32_MAX;
    scratchBuffer_.Reset();
}

bool GpuSphManager::CreateSpatialHashBuffers(uint32_t particleCapacity)
{
    if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr || particleCapacity == 0)
    {
        return false;
    }

    hashEntryCapacity_ = GetSortCount(particleCapacity);
    cellRangeCapacity_ = kMaxSpatialCellCapacity;
    constexpr uint32_t kPairStride = sizeof(uint32_t) * 2;

    hashEntriesBuffer_ = ResourceManager::CreateBufferResource(
        dxCommon_->GetDevice(),
        static_cast<uint64_t>(hashEntryCapacity_) * kPairStride,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cellRangesBuffer_ = ResourceManager::CreateBufferResource(
        dxCommon_->GetDevice(),
        static_cast<uint64_t>(cellRangeCapacity_) * kPairStride,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (!hashEntriesBuffer_ || !cellRangesBuffer_)
    {
        ReleaseSpatialHashBuffers();
        return false;
    }

    hashEntriesBuffer_->SetName(L"GpuSph.W6.HashEntries");
    cellRangesBuffer_->SetName(L"GpuSph.W6.CellRanges");

    try
    {
        hashEntriesUavIndex_ = UAVManager::GetInstance()->Allocate();
        cellRangesUavIndex_ = UAVManager::GetInstance()->Allocate();
        UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
            hashEntriesUavIndex_, hashEntriesBuffer_.Get(), hashEntryCapacity_, kPairStride);
        UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
            cellRangesUavIndex_, cellRangesBuffer_.Get(), cellRangeCapacity_, kPairStride);
    }
    catch (...)
    {
        ReleaseSpatialHashBuffers();
        return false;
    }

    return true;
}

void GpuSphManager::ReleaseSpatialHashBuffers()
{
    if (hashEntriesUavIndex_ != UINT32_MAX)
    {
        UAVManager::GetInstance()->Free(hashEntriesUavIndex_);
    }
    if (cellRangesUavIndex_ != UINT32_MAX)
    {
        UAVManager::GetInstance()->Free(cellRangesUavIndex_);
    }

    hashEntriesUavIndex_ = UINT32_MAX;
    cellRangesUavIndex_ = UINT32_MAX;
    hashEntryCapacity_ = 0;
    cellRangeCapacity_ = 0;
    hashEntriesBuffer_.Reset();
    cellRangesBuffer_.Reset();
}

bool GpuSphManager::CreateCflReadbackBuffers()
{
    if (!dxCommon_ || !dxCommon_->GetDevice() || !dxCommon_->GetCommandManager())
    {
        return false;
    }

    const uint32_t frameCount = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
    cflReadbackSlots_.resize(frameCount);
    for (uint32_t index = 0; index < frameCount; ++index)
    {
        CflReadbackSlot& slot = cflReadbackSlots_[index];
        slot.buffer = ResourceManager::CreateBufferResource(
            dxCommon_->GetDevice(),
            sizeof(uint32_t),
            D3D12_HEAP_TYPE_READBACK,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        if (!slot.buffer)
        {
            ReleaseCflReadbackBuffers();
            return false;
        }
        const std::wstring name = L"GpuSph.W9.5.CflReadback." + std::to_wstring(index);
        slot.buffer->SetName(name.c_str());
    }

    stats_.frameResourceCount = frameCount;
    return true;
}

void GpuSphManager::ReleaseCflReadbackBuffers()
{
    for (CflReadbackSlot& slot : cflReadbackSlots_)
    {
        slot.buffer.Reset();
        slot.pending = false;
    }
    cflReadbackSlots_.clear();
}

void GpuSphManager::ConsumeCflReadback()
{
    if (!dxCommon_ || !dxCommon_->GetCommandManager() || cflReadbackSlots_.empty())
    {
        return;
    }

    const uint32_t frameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
    if (frameIndex >= cflReadbackSlots_.size())
    {
        return;
    }

    CflReadbackSlot& slot = cflReadbackSlots_[frameIndex];
    if (!slot.pending || !slot.buffer)
    {
        return;
    }

    uint32_t* mappedValue = nullptr;
    D3D12_RANGE readRange{ 0, sizeof(uint32_t) };
    if (SUCCEEDED(slot.buffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedValue))) && mappedValue)
    {
        lastMeasuredMaxSpeed_ = static_cast<float>(*mappedValue) / kCflMetricScale;
        D3D12_RANGE writeRange{ 0, 0 };
        slot.buffer->Unmap(0, &writeRange);
        ++stats_.cflReadbackCount;
    }
    slot.pending = false;
}

bool GpuSphManager::ScheduleCflReadback()
{
    if (!dxCommon_ || !dxCommon_->GetCommandManager() || cflReadbackSlots_.empty() || !hashEntriesBuffer_)
    {
        return false;
    }

    DX12CommandManager* commandManager = dxCommon_->GetCommandManager();
    ID3D12GraphicsCommandList* commandList = commandManager->GetCommandList();
    const uint32_t frameIndex = commandManager->GetCurrentFrameIndex();
    if (!commandList || frameIndex >= cflReadbackSlots_.size())
    {
        return false;
    }

    const uint32_t activeCount = GetValidatedActiveParticleCount();
    if (activeCount == 0)
    {
        return true;
    }

    const GpuSphSimulationConstants constants = BuildConstants(effectiveDeltaTime_);
    const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
    if (!allocation.IsValid())
    {
        return false;
    }

    const GpuSphDispatchConstants defaults{};
    if (!DispatchStage(GpuSphComputeShaderId::CflMetricClear, allocation.gpuAddress, 1u, defaults, false, false, true, false))
    {
        return false;
    }
    if (!DispatchStage(GpuSphComputeShaderId::CflMetricMeasure, allocation.gpuAddress, activeCount, defaults, false, false, true, false))
    {
        return false;
    }
    stats_.cflMetricDispatchCount += 2;

    TransitionHashBuffer(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->CopyBufferRegion(cflReadbackSlots_[frameIndex].buffer.Get(), 0, hashEntriesBuffer_.Get(), 0, sizeof(uint32_t));
    TransitionHashBuffer(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cflReadbackSlots_[frameIndex].pending = true;
    return true;
}

bool GpuSphManager::ExecuteReset()
{
    if (!initialized_ && (dxCommon_ == nullptr || rootSignature_ == nullptr))
    {
        return false;
    }

    const uint32_t activeCount = GetValidatedActiveParticleCount();
    particleBuffer_.SetActiveParticleCount(activeCount);
    if (activeCount == 0)
    {
        ++stats_.resetCount;
        return true;
    }

    const GpuSphSimulationConstants constants = BuildConstants(effectiveDeltaTime_);
    const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
    if (!allocation.IsValid())
    {
        return false;
    }

    particleBuffer_.Transition(
        dxCommon_->GetCommandManager()->GetCommandList(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    UAVManager::GetInstance()->PreDispatch();

    const GpuSphDispatchConstants dispatchConstants{};
    const bool succeeded = DispatchStage(
        GpuSphComputeShaderId::Reset,
        allocation.gpuAddress,
        activeCount,
        dispatchConstants,
        true,
        true,
        false,
        false);
    if (succeeded)
    {
        ++stats_.resetCount;
        stats_.spatialHashReady = false;
    }
    return succeeded;
}

bool GpuSphManager::ExecuteSimulationStep(float deltaTime)
{
    const uint32_t activeCount = GetValidatedActiveParticleCount();
    particleBuffer_.SetActiveParticleCount(activeCount);
    if (activeCount == 0)
    {
        return true;
    }

    const GpuSphSimulationConstants constants = BuildConstants(deltaTime);
    if (constants.spatialCellCount == 0)
    {
        stats_.spatialHashReady = false;
        return false;
    }

    const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
    if (!allocation.IsValid())
    {
        return false;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
    if (commandList == nullptr)
    {
        return false;
    }

    particleBuffer_.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    UAVManager::GetInstance()->PreDispatch();
    const GpuSphDispatchConstants defaults{};

    if (!DispatchStage(GpuSphComputeShaderId::Gravity, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.gravityDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::Predict, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.predictionDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::BoundaryPredicted, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.boundaryDispatchCount;

    // W6では予測位置をKey化・GPU Sortし、各Cellの連続Rangeを構築してからSPH近傍計算へ進む。
    if (!ExecuteSpatialHashBuild(allocation.gpuAddress)) return false;

    if (!DispatchStage(GpuSphComputeShaderId::Density, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.densityDispatchCount;

    if (settings_.dfsphEnabled)
    {
        if (!ExecuteDfSphProjection(allocation.gpuAddress, activeCount)) return false;
    }
    else
    {
        stats_.lastDensityIterations = 0;
        stats_.lastDivergenceIterations = 0;
        if (!DispatchStage(GpuSphComputeShaderId::PressureProperty, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
        if (!DispatchStage(GpuSphComputeShaderId::PressureForce, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
        stats_.pressureDispatchCount += 2;
    }

    if (!DispatchStage(GpuSphComputeShaderId::ViscosityDelta, allocation.gpuAddress, activeCount, defaults, false, true, false, false)) return false;
    if (!DispatchStage(GpuSphComputeShaderId::ViscosityApply, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    stats_.viscosityDispatchCount += 2;

    if (!DispatchStage(GpuSphComputeShaderId::Integrate, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.predictionDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::BoundaryPosition, allocation.gpuAddress, activeCount, defaults, true, false, false, false)) return false;
    ++stats_.boundaryDispatchCount;

    ++stats_.totalSimulationSteps;
    return true;
}

bool GpuSphManager::ExecuteDfSphProjection(
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
    uint32_t activeCount)
{
    const GpuSphDispatchConstants defaults{};
    if (!DispatchStage(
        GpuSphComputeShaderId::DfSphFactor,
        constantBufferAddress,
        activeCount,
        defaults,
        false,
        true,
        false,
        false))
    {
        return false;
    }
    ++stats_.dfsphFactorDispatchCount;

    const uint32_t densityIterations = (std::clamp)(settings_.dfsphDensityIterations, 1u, 12u);
    const uint32_t divergenceIterations = (std::clamp)(settings_.dfsphDivergenceIterations, 0u, 12u);

    for (uint32_t iteration = 0; iteration < densityIterations; ++iteration)
    {
        if (!DispatchStage(GpuSphComputeShaderId::DfSphDensityPrepare, constantBufferAddress, activeCount, defaults, true, false, false, false)) return false;
        if (!DispatchStage(GpuSphComputeShaderId::DfSphDensityApply, constantBufferAddress, activeCount, defaults, true, false, false, false)) return false;
        stats_.dfsphDensityDispatchCount += 2;
    }

    for (uint32_t iteration = 0; iteration < divergenceIterations; ++iteration)
    {
        if (!DispatchStage(GpuSphComputeShaderId::DfSphDivergencePrepare, constantBufferAddress, activeCount, defaults, true, false, false, false)) return false;
        if (!DispatchStage(GpuSphComputeShaderId::DfSphDivergenceApply, constantBufferAddress, activeCount, defaults, true, false, false, false)) return false;
        stats_.dfsphDivergenceDispatchCount += 2;
    }

    stats_.lastDensityIterations = densityIterations;
    stats_.lastDivergenceIterations = divergenceIterations;
    return true;
}

bool GpuSphManager::ExecuteSpatialHashBuild(D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress)
{
    const uint32_t activeCount = GetValidatedActiveParticleCount();
    const uint32_t sortCount = GetSortCount(activeCount);
    const SpatialGridDesc grid = BuildSpatialGridDesc(settings_, cellRangeCapacity_);
    stats_.spatialHashReady = false;

    if (activeCount == 0 || sortCount == 0 || sortCount > hashEntryCapacity_ || !grid.valid)
    {
        return false;
    }

    GpuSphDispatchConstants dispatchConstants{};
    dispatchConstants.sortCount = sortCount;
    dispatchConstants.cellCount = grid.cellCount;

    if (!DispatchStage(
        GpuSphComputeShaderId::SpatialBuildKeys,
        constantBufferAddress,
        sortCount,
        dispatchConstants,
        false,
        false,
        true,
        false))
    {
        return false;
    }

    for (uint32_t sortLevel = 2; sortLevel <= sortCount; sortLevel <<= 1u)
    {
        for (uint32_t sortMask = sortLevel >> 1u; sortMask > 0; sortMask >>= 1u)
        {
            dispatchConstants.sortLevel = sortLevel;
            dispatchConstants.sortLevelMask = sortMask;
            if (!DispatchStage(
                GpuSphComputeShaderId::SpatialBitonicSort,
                constantBufferAddress,
                sortCount,
                dispatchConstants,
                false,
                false,
                true,
                false))
            {
                return false;
            }
            ++stats_.spatialHashSortDispatchCount;
        }

        if (sortLevel == sortCount)
        {
            break;
        }
    }

    dispatchConstants.sortLevel = 0;
    dispatchConstants.sortLevelMask = 0;
    if (!DispatchStage(
        GpuSphComputeShaderId::SpatialClearCellRanges,
        constantBufferAddress,
        grid.cellCount,
        dispatchConstants,
        false,
        false,
        false,
        true))
    {
        return false;
    }
    ++stats_.cellRangeDispatchCount;

    if (!DispatchStage(
        GpuSphComputeShaderId::SpatialBuildCellRanges,
        constantBufferAddress,
        activeCount,
        dispatchConstants,
        false,
        false,
        false,
        true))
    {
        return false;
    }
    ++stats_.cellRangeDispatchCount;

    ++stats_.spatialHashBuildCount;
    stats_.sortedParticleCount = sortCount;
    stats_.spatialGridDimX = grid.dimX;
    stats_.spatialGridDimY = grid.dimY;
    stats_.spatialGridDimZ = grid.dimZ;
    stats_.spatialCellCount = grid.cellCount;
    stats_.spatialCellSize = grid.cellSize;
    stats_.spatialHashReady = true;
    return true;
}

bool GpuSphManager::DispatchStage(
    GpuSphComputeShaderId shaderId,
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
    uint32_t dispatchItemCount,
    const GpuSphDispatchConstants& dispatchConstants,
    bool particleBarrier,
    bool scratchBarrier,
    bool hashBarrier,
    bool cellRangeBarrier)
{
    const std::size_t pipelineIndex = static_cast<std::size_t>(shaderId);
    if (dispatchItemCount == 0)
    {
        return true;
    }
    if (pipelineIndex >= pipelineStates_.size() ||
        !pipelineStates_[pipelineIndex] ||
        !rootSignature_ ||
        !scratchBuffer_ ||
        !hashEntriesBuffer_ ||
        !cellRangesBuffer_ ||
        scratchUavIndex_ == UINT32_MAX ||
        hashEntriesUavIndex_ == UINT32_MAX ||
        cellRangesUavIndex_ == UINT32_MAX)
    {
        return false;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
    if (commandList == nullptr)
    {
        return false;
    }

    UAVManager* descriptors = UAVManager::GetInstance();
    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[pipelineIndex].Get());
    commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
    commandList->SetComputeRootDescriptorTable(1, descriptors->GetGPUDescriptorHandle(particleBuffer_.GetUavIndex()));
    commandList->SetComputeRootDescriptorTable(2, descriptors->GetGPUDescriptorHandle(scratchUavIndex_));
    commandList->SetComputeRootDescriptorTable(3, descriptors->GetGPUDescriptorHandle(hashEntriesUavIndex_));
    commandList->SetComputeRootDescriptorTable(4, descriptors->GetGPUDescriptorHandle(cellRangesUavIndex_));
    commandList->SetComputeRoot32BitConstants(5, 4, &dispatchConstants, 0);

    const uint32_t groupCount = (dispatchItemCount + kThreadGroupSize - 1u) / kThreadGroupSize;
    commandList->Dispatch(groupCount, 1, 1);

    if (particleBarrier)
    {
        particleBuffer_.InsertUavBarrier(commandList);
    }
    if (scratchBarrier)
    {
        InsertUavBarrier(scratchBuffer_.Get());
    }
    if (hashBarrier)
    {
        InsertUavBarrier(hashEntriesBuffer_.Get());
    }
    if (cellRangeBarrier)
    {
        InsertUavBarrier(cellRangesBuffer_.Get());
    }

    ++stats_.totalDispatchCount;
    return true;
}

GpuSphManager::GpuSphSimulationConstants GpuSphManager::BuildConstants(float deltaTime) const
{
    GpuSphSimulationConstants constants{};
    constants.activeParticleCount = GetValidatedActiveParticleCount();
    constants.deltaTime = deltaTime;
    constants.particleMass = (std::max)(settings_.particleMass, 1.0e-6f);
    constants.smoothingRadius = (std::max)(settings_.smoothingRadius, 0.001f);
    constants.targetDensity = (std::max)(settings_.targetDensity, 1.0e-3f);
    constants.pressureStiffness = (std::max)(settings_.pressureStiffness, 0.0f);
    constants.viscosityStrength = (std::max)(settings_.viscosityStrength, 0.0f);
    constants.boundaryDamping = (std::clamp)(settings_.boundaryDamping, 0.0f, 1.0f);
    constants.gravity = settings_.gravity;
    constants.boundaryMin = settings_.boundaryMin;
    constants.boundaryMax = settings_.boundaryMax;
    constants.spawnOrigin = settings_.spawnOrigin;
    constants.spawnSpacing = (std::max)(settings_.spawnSpacing, 0.001f);
    constants.spawnDimX = (std::max)(settings_.spawnDimX, 1u);
    constants.spawnDimY = (std::max)(settings_.spawnDimY, 1u);
    constants.spawnDimZ = (std::max)(settings_.spawnDimZ, 1u);

    const SpatialGridDesc grid = BuildSpatialGridDesc(settings_, cellRangeCapacity_);
    if (grid.valid)
    {
        constants.spatialGridMin = grid.min;
        constants.spatialCellSize = grid.cellSize;
        constants.spatialGridDimX = grid.dimX;
        constants.spatialGridDimY = grid.dimY;
        constants.spatialGridDimZ = grid.dimZ;
        constants.spatialCellCount = grid.cellCount;
    }

    constants.dfsphEnabled = settings_.dfsphEnabled ? 1u : 0u;
    constants.dfsphDensityIterations = (std::clamp)(settings_.dfsphDensityIterations, 1u, 12u);
    constants.dfsphDivergenceIterations = (std::clamp)(settings_.dfsphDivergenceIterations, 0u, 12u);
    constants.adaptiveCflEnabled = settings_.adaptiveCflEnabled ? 1u : 0u;
    constants.dfsphDensityRelaxation = (std::clamp)(settings_.dfsphDensityRelaxation, 0.0f, 1.0f);
    constants.dfsphDivergenceRelaxation = (std::clamp)(settings_.dfsphDivergenceRelaxation, 0.0f, 1.0f);
    constants.dfsphDensityErrorTolerance = (std::max)(settings_.dfsphDensityErrorTolerance, 0.0f);
    constants.dfsphDivergenceErrorTolerance = (std::max)(settings_.dfsphDivergenceErrorTolerance, 0.0f);
    constants.cflNumber = (std::clamp)(settings_.cflNumber, 0.05f, 0.95f);
    constants.minimumDeltaTime = (std::clamp)(settings_.minimumDeltaTime, 1.0f / 2000.0f, settings_.fixedDeltaTime);
    constants.surfaceTensionStrength = (std::max)(settings_.surfaceTensionStrength, 0.0f);
    constants.xsphStrength = (std::clamp)(settings_.xsphStrength, 0.0f, 1.0f);
    constants.boundaryFriction = (std::clamp)(settings_.boundaryFriction, 0.0f, 1.0f);
    constants.maxDfsphVelocityCorrection = (std::max)(settings_.maxDfsphVelocityCorrection, 0.01f);
    return constants;
}

uint32_t GpuSphManager::GetValidatedActiveParticleCount() const
{
    return (std::min)(settings_.activeParticleCount, particleBuffer_.GetCapacity());
}

uint32_t GpuSphManager::GetSortCount(uint32_t activeCount) const
{
    if (activeCount == 0)
    {
        return 0;
    }

    uint32_t result = 1;
    while (result < activeCount && result <= (1u << 30))
    {
        result <<= 1u;
    }
    return result;
}

float GpuSphManager::CalculateEffectiveDeltaTime(float requestedDeltaTime) const
{
    const float safeRequested = (std::max)(requestedDeltaTime, 1.0f / 2000.0f);
    if (!settings_.adaptiveCflEnabled)
    {
        return safeRequested;
    }

    float referenceSpeed = lastMeasuredMaxSpeed_;
    if (referenceSpeed <= 0.1f)
    {
        const float domainHeight = (std::max)(settings_.boundaryMax.y - settings_.boundaryMin.y, settings_.smoothingRadius);
        const float gravityMagnitude = Vector3::Length(settings_.gravity);
        referenceSpeed = std::sqrt((std::max)(2.0f * gravityMagnitude * domainHeight, 0.01f));
    }

    const float cflDeltaTime =
        (std::clamp)(settings_.cflNumber, 0.05f, 0.95f) *
        (std::max)(settings_.smoothingRadius, 0.001f) /
        (std::max)(referenceSpeed, 0.1f);
    const float minimumDeltaTime = (std::clamp)(settings_.minimumDeltaTime, 1.0f / 2000.0f, safeRequested);
    return (std::clamp)(cflDeltaTime, minimumDeltaTime, safeRequested);
}

void GpuSphManager::UpdateSpawnLayoutForActiveCount(uint32_t activeCount)
{
    if (activeCount == 0)
    {
        return;
    }

    const uint32_t side = (std::max)(1u, static_cast<uint32_t>(std::ceil(std::cbrt(static_cast<double>(activeCount)))));
    settings_.spawnDimX = side;
    settings_.spawnDimY = side;
    const uint64_t slice = static_cast<uint64_t>(side) * side;
    settings_.spawnDimZ = static_cast<uint32_t>((activeCount + slice - 1ull) / slice);

    float spacing = (std::max)(settings_.spawnSpacing, 0.001f);
    const float extentX = (std::max)(settings_.boundaryMax.x - settings_.boundaryMin.x, 0.001f);
    const float extentY = (std::max)(settings_.boundaryMax.y - settings_.boundaryMin.y, 0.001f);
    const float extentZ = (std::max)(settings_.boundaryMax.z - settings_.boundaryMin.z, 0.001f);

    const float fitX = settings_.spawnDimX > 1 ? extentX * 0.95f / static_cast<float>(settings_.spawnDimX - 1u) : spacing;
    const float fitY = settings_.spawnDimY > 1 ? extentY * 0.95f / static_cast<float>(settings_.spawnDimY - 1u) : spacing;
    const float fitZ = settings_.spawnDimZ > 1 ? extentZ * 0.95f / static_cast<float>(settings_.spawnDimZ - 1u) : spacing;
    spacing = (std::max)(0.001f, (std::min)(spacing, (std::min)(fitX, (std::min)(fitY, fitZ))));
    settings_.spawnSpacing = spacing;

    const float spanX = static_cast<float>(settings_.spawnDimX - 1u) * spacing;
    const float spanZ = static_cast<float>(settings_.spawnDimZ - 1u) * spacing;
    settings_.spawnOrigin.x = (settings_.boundaryMin.x + settings_.boundaryMax.x - spanX) * 0.5f;
    settings_.spawnOrigin.y = settings_.boundaryMin.y + spacing * 0.5f;
    settings_.spawnOrigin.z = (settings_.boundaryMin.z + settings_.boundaryMax.z - spanZ) * 0.5f;
}

void GpuSphManager::InsertUavBarrier(ID3D12Resource* resource) const
{
    if (resource == nullptr || dxCommon_ == nullptr)
    {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
    if (commandList == nullptr)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    commandList->ResourceBarrier(1, &barrier);
}

void GpuSphManager::TransitionHashBuffer(
    ID3D12GraphicsCommandList* commandList,
    D3D12_RESOURCE_STATES beforeState,
    D3D12_RESOURCE_STATES afterState) const
{
    if (!commandList || !hashEntriesBuffer_ || beforeState == afterState)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = hashEntriesBuffer_.Get();
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

void GpuSphManager::RefreshStats(uint32_t substeps, bool lastStepSucceeded)
{
    stats_.lastFrameSubsteps = substeps;
    stats_.accumulatorSeconds = accumulatorSeconds_;
    stats_.effectiveDeltaTime = effectiveDeltaTime_;
    stats_.lastMeasuredMaxSpeed = lastMeasuredMaxSpeed_;
    stats_.initialized = initialized_;
    stats_.paused = paused_;
    stats_.lastStepSucceeded = lastStepSucceeded;
    stats_.dfsphActive = settings_.dfsphEnabled;
    stats_.frameResourceCount = static_cast<uint32_t>(cflReadbackSlots_.size());
    if (settings_.adaptiveCflEnabled && effectiveDeltaTime_ < settings_.fixedDeltaTime)
    {
        ++stats_.cflStabilizationCount;
    }
    stats_.approximateGpuMemoryBytes =
        particleBuffer_.GetApproximateGpuMemoryBytes() +
        static_cast<uint64_t>(particleBuffer_.GetCapacity()) * sizeof(float) * 4ull +
        static_cast<uint64_t>(hashEntryCapacity_) * sizeof(uint32_t) * 2ull +
        static_cast<uint64_t>(cellRangeCapacity_) * sizeof(uint32_t) * 2ull;

    const SpatialGridDesc grid = BuildSpatialGridDesc(settings_, cellRangeCapacity_);
    if (grid.valid)
    {
        stats_.spatialGridDimX = grid.dimX;
        stats_.spatialGridDimY = grid.dimY;
        stats_.spatialGridDimZ = grid.dimZ;
        stats_.spatialCellCount = grid.cellCount;
        stats_.spatialCellSize = grid.cellSize;
    }
    else
    {
        stats_.spatialHashReady = false;
        stats_.spatialGridDimX = 0;
        stats_.spatialGridDimY = 0;
        stats_.spatialGridDimZ = 0;
        stats_.spatialCellCount = 0;
        stats_.spatialCellSize = 0.0f;
    }
}

} // namespace Ken4lowEngine
