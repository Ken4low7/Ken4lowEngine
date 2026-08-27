#pragma once

#include "Engine/Graphics/Renderer/GpuFluid/Sph/Resource/GpuSphParticleBuffer.h"

#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <ShaderCompiler.h>
#include <ShaderManifestTypes.h>
#include <UAVManager.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

struct GpuProductionLiquidSecondaryClassifierStats
{
    uint64_t classifyDispatchCount = 0;
    uint64_t readbackCount = 0;
    uint32_t sprayCandidateCount = 0;
    uint32_t foamCandidateCount = 0;
    uint32_t bubbleCandidateCount = 0;
    uint32_t classifiedParticleCount = 0;
    bool initialized = false;
    bool lastDispatchSucceeded = true;
};

/// Primary SPHからSpray/Foam/Bubble候補をGPU分類し、Counterだけを非同期Readbackする。
class GpuProductionLiquidSecondaryClassifier final
{
public:
    static GpuProductionLiquidSecondaryClassifier* GetInstance()
    {
        static GpuProductionLiquidSecondaryClassifier instance;
        return &instance;
    }

    bool Initialize()
    {
        Finalize();
        dxCommon_ = DirectXCommon::GetInstance();
        if (!dxCommon_ || !dxCommon_->GetDevice() || !dxCommon_->GetCommandManager())
        {
            return false;
        }

        constexpr uint32_t kCounterCount = 4;
        constexpr uint32_t kCounterStride = sizeof(uint32_t);
        counterBuffer_ = ResourceManager::CreateBufferResource(
            dxCommon_->GetDevice(),
            static_cast<uint64_t>(kCounterCount) * kCounterStride,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (!counterBuffer_)
        {
            Finalize();
            return false;
        }
        counterBuffer_->SetName(L"GpuLiquid.SecondaryCounters");

        try
        {
            counterUavIndex_ = UAVManager::GetInstance()->Allocate();
            UAVManager::GetInstance()->CreateUAVForStructuredBuffer(
                counterUavIndex_, counterBuffer_.Get(), kCounterCount, kCounterStride);
        }
        catch (...)
        {
            Finalize();
            return false;
        }

        const uint32_t frameCount = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
        readbackSlots_.resize(frameCount);
        for (uint32_t index = 0; index < frameCount; ++index)
        {
            ReadbackSlot& slot = readbackSlots_[index];
            slot.buffer = ResourceManager::CreateBufferResource(
                dxCommon_->GetDevice(),
                sizeof(CounterReadback),
                D3D12_HEAP_TYPE_READBACK,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COPY_DEST);
            if (!slot.buffer)
            {
                Finalize();
                return false;
            }
            const std::wstring name = L"GpuLiquid.SecondaryReadback." + std::to_wstring(index);
            slot.buffer->SetName(name.c_str());
        }

        if (!CreateRootSignature() || !CreatePipelineState(L"ClearCounters", clearPso_) || !CreatePipelineState(L"Classify", classifyPso_))
        {
            Finalize();
            return false;
        }

        initialized_ = true;
        stats_.initialized = true;
        return true;
    }

    void Finalize()
    {
        clearPso_.Reset();
        classifyPso_.Reset();
        rootSignature_.Reset();
        for (ReadbackSlot& slot : readbackSlots_)
        {
            slot.buffer.Reset();
            slot.pending = false;
        }
        readbackSlots_.clear();
        if (counterUavIndex_ != UINT32_MAX)
        {
            UAVManager::GetInstance()->Free(counterUavIndex_);
        }
        counterUavIndex_ = UINT32_MAX;
        counterBuffer_.Reset();
        counterState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        dxCommon_ = nullptr;
        initialized_ = false;
        frameCounter_ = 0;
        stats_ = {};
    }

    bool Update(
        GpuSphParticleBuffer& particles,
        bool enabled,
        uint32_t intervalFrames,
        float targetDensity,
        float spraySpeedThreshold,
        float foamSpeedThreshold,
        float freeSurfaceDensityRatio,
        float bubbleDensityRatio)
    {
        if (!initialized_ && !Initialize())
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        ConsumeReadback();
        ++frameCounter_;
        intervalFrames = (std::max)(intervalFrames, 1u);
        if (!enabled || !particles.IsInitialized() || particles.GetActiveParticleCount() == 0 ||
            (frameCounter_ % intervalFrames) != 0u)
        {
            stats_.lastDispatchSucceeded = true;
            return true;
        }

        ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
        if (!commandList || counterUavIndex_ == UINT32_MAX)
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        ClassifyConstants constants{};
        constants.activeParticleCount = particles.GetActiveParticleCount();
        constants.targetDensity = (std::max)(targetDensity, 1.0e-5f);
        constants.spraySpeedThreshold = (std::max)(spraySpeedThreshold, 0.0f);
        constants.foamSpeedThreshold = (std::max)(foamSpeedThreshold, 0.0f);
        constants.freeSurfaceDensityRatio = (std::clamp)(freeSurfaceDensityRatio, 0.1f, 1.2f);
        constants.bubbleDensityRatio = (std::max)(bubbleDensityRatio, 0.1f);
        const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
        if (!allocation.IsValid())
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        particles.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        TransitionCounter(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        UAVManager::GetInstance()->PreDispatch();

        commandList->SetComputeRootSignature(rootSignature_.Get());
        commandList->SetComputeRootConstantBufferView(0, allocation.gpuAddress);
        commandList->SetComputeRootDescriptorTable(
            1, UAVManager::GetInstance()->GetGPUDescriptorHandle(particles.GetComputeSrvIndex()));
        commandList->SetComputeRootDescriptorTable(
            2, UAVManager::GetInstance()->GetGPUDescriptorHandle(counterUavIndex_));

        commandList->SetPipelineState(clearPso_.Get());
        commandList->Dispatch(1, 1, 1);
        InsertCounterBarrier(commandList);

        commandList->SetPipelineState(classifyPso_.Get());
        const uint32_t groupCount = (particles.GetActiveParticleCount() + 127u) / 128u;
        commandList->Dispatch(groupCount, 1, 1);
        InsertCounterBarrier(commandList);
        ++stats_.classifyDispatchCount;

        const uint32_t frameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
        if (frameIndex < readbackSlots_.size() && readbackSlots_[frameIndex].buffer)
        {
            TransitionCounter(commandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
            commandList->CopyBufferRegion(
                readbackSlots_[frameIndex].buffer.Get(), 0,
                counterBuffer_.Get(), 0,
                sizeof(CounterReadback));
            TransitionCounter(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            readbackSlots_[frameIndex].pending = true;
        }

        stats_.lastDispatchSucceeded = true;
        return true;
    }

    [[nodiscard]] bool IsInitialized() const { return initialized_; }
    [[nodiscard]] const GpuProductionLiquidSecondaryClassifierStats& GetStats() const { return stats_; }

private:
    struct ClassifyConstants
    {
        uint32_t activeParticleCount = 0;
        float targetDensity = 1000.0f;
        float spraySpeedThreshold = 3.5f;
        float foamSpeedThreshold = 1.5f;
        float freeSurfaceDensityRatio = 0.88f;
        float bubbleDensityRatio = 1.08f;
        float padding0 = 0.0f;
        float padding1 = 0.0f;
    };
    static_assert(sizeof(ClassifyConstants) == 32);

    struct CounterReadback
    {
        uint32_t spray = 0;
        uint32_t foam = 0;
        uint32_t bubble = 0;
        uint32_t classified = 0;
    };
    static_assert(sizeof(CounterReadback) == 16);

    struct ReadbackSlot
    {
        ComPtr<ID3D12Resource> buffer{};
        bool pending = false;
    };

    bool CreateRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE particleRange{};
        particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        particleRange.NumDescriptors = 1;
        particleRange.BaseShaderRegister = 0;
        particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE counterRange{};
        counterRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        counterRange.NumDescriptors = 1;
        counterRange.BaseShaderRegister = 0;
        counterRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER params[3]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &particleRange;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable.NumDescriptorRanges = 1;
        params[2].DescriptorTable.pDescriptorRanges = &counterRange;
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = _countof(params);
        desc.pParameters = params;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            return false;
        }
        return SUCCEEDED(dxCommon_->GetDevice()->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)));
    }

    bool CreatePipelineState(const wchar_t* entryPoint, ComPtr<ID3D12PipelineState>& output)
    {
        const ShaderDescriptor descriptor{
            L"GpuLiquidSecondaryCS",
            L"Resources/Shaders/GpuFluid/Sph/Production/GpuSphSecondaryClassify.CS.hlsl",
            entryPoint,
            L"cs_6_0",
            ShaderStage::Compute,
            RootSignatureType::Compute };
        ComPtr<IDxcBlob> shader = ShaderCompiler::CompileShader(descriptor, dxCommon_->GetDXCCompilerManager());
        if (!shader)
        {
            return false;
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = rootSignature_.Get();
        desc.CS = { shader->GetBufferPointer(), shader->GetBufferSize() };
        return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&output)));
    }

    void ConsumeReadback()
    {
        if (!dxCommon_ || readbackSlots_.empty())
        {
            return;
        }
        const uint32_t frameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
        if (frameIndex >= readbackSlots_.size())
        {
            return;
        }
        ReadbackSlot& slot = readbackSlots_[frameIndex];
        if (!slot.pending || !slot.buffer)
        {
            return;
        }

        CounterReadback* mapped = nullptr;
        D3D12_RANGE readRange{ 0, sizeof(CounterReadback) };
        if (SUCCEEDED(slot.buffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped))) && mapped)
        {
            stats_.sprayCandidateCount = mapped->spray;
            stats_.foamCandidateCount = mapped->foam;
            stats_.bubbleCandidateCount = mapped->bubble;
            stats_.classifiedParticleCount = mapped->classified;
            D3D12_RANGE writeRange{ 0, 0 };
            slot.buffer->Unmap(0, &writeRange);
            ++stats_.readbackCount;
        }
        slot.pending = false;
    }

    void TransitionCounter(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState)
    {
        if (!commandList || !counterBuffer_ || counterState_ == nextState)
        {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = counterBuffer_.Get();
        barrier.Transition.StateBefore = counterState_;
        barrier.Transition.StateAfter = nextState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        counterState_ = nextState;
    }

    void InsertCounterBarrier(ID3D12GraphicsCommandList* commandList) const
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = counterBuffer_.Get();
        commandList->ResourceBarrier(1, &barrier);
    }

    GpuProductionLiquidSecondaryClassifier() = default;

    DirectXCommon* dxCommon_ = nullptr;
    ComPtr<ID3D12Resource> counterBuffer_{};
    ComPtr<ID3D12RootSignature> rootSignature_{};
    ComPtr<ID3D12PipelineState> clearPso_{};
    ComPtr<ID3D12PipelineState> classifyPso_{};
    std::vector<ReadbackSlot> readbackSlots_{};
    uint32_t counterUavIndex_ = UINT32_MAX;
    D3D12_RESOURCE_STATES counterState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    uint64_t frameCounter_ = 0;
    GpuProductionLiquidSecondaryClassifierStats stats_{};
    bool initialized_ = false;
};

} // namespace Ken4lowEngine
