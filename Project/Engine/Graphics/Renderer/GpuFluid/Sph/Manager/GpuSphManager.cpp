#include "GpuSphManager.h"

#include <DirectXCommon.h>
#include <GpuSphShaderManifest.h>
#include <LogString.h>
#include <ResourceManager.h>
#include <ShaderCompiler.h>
#include <UAVManager.h>

#include <algorithm>
#include <string>

namespace Ken4lowEngine
{

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

    if (!particleBuffer_.Initialize(particleCapacity) ||
        !CreateScratchBuffer(particleCapacity) ||
        !CreateRootSignature() ||
        !CreatePipelineStates())
    {
        Finalize();
        return false;
    }

    // W6導入前はO(N^2)近傍探索なので実行粒子数だけを安全域へ制限する。
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
    ReleaseScratchBuffer();
    particleBuffer_.Finalize();

    settings_ = {};
    stats_ = {};
    accumulatorSeconds_ = 0.0f;
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

    const float fixedDeltaTime = (std::max)(settings_.fixedDeltaTime, 1.0f / 1000.0f);
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

        // 長い停止復帰でも無制限にCatch-upせず、次Frameへ最大1Frame分だけ持ち越す。
        accumulatorSeconds_ = (std::min)(accumulatorSeconds_, fixedDeltaTime * static_cast<float>(maxSubsteps));
    }

    RefreshStats(substeps, stepSucceeded);
}

void GpuSphManager::SetActiveParticleCount(uint32_t activeCount)
{
    settings_.activeParticleCount = activeCount;
    const uint32_t validatedCount = GetValidatedActiveParticleCount();
    settings_.activeParticleCount = validatedCount;
    particleBuffer_.SetActiveParticleCount(validatedCount);
    resetRequested_ = true;
}

bool GpuSphManager::CreateRootSignature()
{
    if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr)
    {
        return false;
    }

    D3D12_ROOT_PARAMETER rootParameters[3]{};
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

    rootSignature_->SetName(L"GpuSph.W5.RootSignature");
    return true;
}

bool GpuSphManager::CreatePipelineStates()
{
    static_assert(static_cast<size_t>(GpuSphComputeShaderId::Count) == kPipelineStateCount);

    for (size_t index = 0; index < kPipelineStateCount; ++index)
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

    scratchBuffer_->SetName(L"GpuSph.W5.VelocityDeltaScratch");
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

    const GpuSphSimulationConstants constants = BuildConstants(settings_.fixedDeltaTime);
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
    const bool succeeded = DispatchStage(
        GpuSphComputeShaderId::Reset,
        allocation.gpuAddress,
        true,
        true);
    if (succeeded)
    {
        ++stats_.resetCount;
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

    if (!DispatchStage(GpuSphComputeShaderId::Gravity, allocation.gpuAddress, true, false)) return false;
    ++stats_.gravityDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::Predict, allocation.gpuAddress, true, false)) return false;
    ++stats_.predictionDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::BoundaryPredicted, allocation.gpuAddress, true, false)) return false;
    ++stats_.boundaryDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::Density, allocation.gpuAddress, true, false)) return false;
    ++stats_.densityDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::PressureProperty, allocation.gpuAddress, true, false)) return false;
    if (!DispatchStage(GpuSphComputeShaderId::PressureForce, allocation.gpuAddress, true, false)) return false;
    stats_.pressureDispatchCount += 2;

    if (!DispatchStage(GpuSphComputeShaderId::ViscosityDelta, allocation.gpuAddress, false, true)) return false;
    if (!DispatchStage(GpuSphComputeShaderId::ViscosityApply, allocation.gpuAddress, true, false)) return false;
    stats_.viscosityDispatchCount += 2;

    if (!DispatchStage(GpuSphComputeShaderId::Integrate, allocation.gpuAddress, true, false)) return false;
    ++stats_.predictionDispatchCount;

    if (!DispatchStage(GpuSphComputeShaderId::BoundaryPosition, allocation.gpuAddress, true, false)) return false;
    ++stats_.boundaryDispatchCount;

    ++stats_.totalSimulationSteps;
    return true;
}

bool GpuSphManager::DispatchStage(
    GpuSphComputeShaderId shaderId,
    D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress,
    bool particleBarrier,
    bool scratchBarrier)
{
    const size_t pipelineIndex = static_cast<size_t>(shaderId);
    if (pipelineIndex >= pipelineStates_.size() ||
        !pipelineStates_[pipelineIndex] ||
        !rootSignature_ ||
        !scratchBuffer_ ||
        scratchUavIndex_ == UINT32_MAX)
    {
        return false;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
    if (commandList == nullptr)
    {
        return false;
    }

    commandList->SetComputeRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineStates_[pipelineIndex].Get());
    commandList->SetComputeRootConstantBufferView(0, constantBufferAddress);
    commandList->SetComputeRootDescriptorTable(
        1,
        UAVManager::GetInstance()->GetGPUDescriptorHandle(particleBuffer_.GetUavIndex()));
    commandList->SetComputeRootDescriptorTable(
        2,
        UAVManager::GetInstance()->GetGPUDescriptorHandle(scratchUavIndex_));

    const uint32_t activeCount = GetValidatedActiveParticleCount();
    const uint32_t groupCount = (activeCount + kThreadGroupSize - 1u) / kThreadGroupSize;
    commandList->Dispatch(groupCount, 1, 1);

    if (particleBarrier)
    {
        particleBuffer_.InsertUavBarrier(commandList);
    }
    if (scratchBarrier)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = scratchBuffer_.Get();
        commandList->ResourceBarrier(1, &barrier);
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
    return constants;
}

uint32_t GpuSphManager::GetValidatedActiveParticleCount() const
{
    const uint64_t spawnCapacity =
        static_cast<uint64_t>((std::max)(settings_.spawnDimX, 1u)) *
        static_cast<uint64_t>((std::max)(settings_.spawnDimY, 1u)) *
        static_cast<uint64_t>((std::max)(settings_.spawnDimZ, 1u));
    const uint32_t bufferCapacity = particleBuffer_.GetCapacity();
    const uint64_t limit = (std::min)(
        static_cast<uint64_t>((std::min)(bufferCapacity, kMaxNaiveNeighborParticles)),
        spawnCapacity);
    return static_cast<uint32_t>((std::min)(static_cast<uint64_t>(settings_.activeParticleCount), limit));
}

void GpuSphManager::RefreshStats(uint32_t substeps, bool lastStepSucceeded)
{
    stats_.lastFrameSubsteps = substeps;
    stats_.accumulatorSeconds = accumulatorSeconds_;
    stats_.initialized = initialized_;
    stats_.paused = paused_;
    stats_.lastStepSucceeded = lastStepSucceeded;
    stats_.approximateGpuMemoryBytes =
        particleBuffer_.GetApproximateGpuMemoryBytes() +
        static_cast<uint64_t>(particleBuffer_.GetCapacity()) * sizeof(float) * 4ull;
}

} // namespace Ken4lowEngine
