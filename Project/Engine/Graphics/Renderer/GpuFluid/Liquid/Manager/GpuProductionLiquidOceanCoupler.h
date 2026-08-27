#pragma once

#include "GpuProductionLiquidOceanBridge.h"
#include "Engine/Graphics/Renderer/GpuFluid/Sph/Resource/GpuSphParticleBuffer.h"

#include <DirectXCommon.h>
#include <ShaderCompiler.h>
#include <ShaderManifestTypes.h>
#include <UAVManager.h>

#include <algorithm>
#include <cstdint>

namespace Ken4lowEngine
{

struct GpuProductionLiquidOceanCouplerStats
{
    uint64_t dispatchCount = 0;
    bool initialized = false;
    bool lastDispatchSucceeded = true;
};

/// Oceanの局所Height / Normal / VelocityをPrimary SPHへGPU上で連成する軽量Coupler。
class GpuProductionLiquidOceanCoupler final
{
public:
    static GpuProductionLiquidOceanCoupler* GetInstance()
    {
        static GpuProductionLiquidOceanCoupler instance;
        return &instance;
    }

    bool Initialize()
    {
        Finalize();
        dxCommon_ = DirectXCommon::GetInstance();
        if (!dxCommon_ || !dxCommon_->GetDevice())
        {
            return false;
        }
        if (!CreateRootSignature() || !CreatePipelineState())
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
        pipelineState_.Reset();
        rootSignature_.Reset();
        dxCommon_ = nullptr;
        stats_ = {};
        initialized_ = false;
    }

    bool Update(
        GpuSphParticleBuffer& particles,
        float deltaTime,
        const GpuProductionLiquidOceanSample& sample,
        float blendBand,
        float velocityCoupling,
        float surfaceAttraction,
        float maxCorrection)
    {
        if (!sample.valid || particles.GetActiveParticleCount() == 0)
        {
            stats_.lastDispatchSucceeded = true;
            return true;
        }
        if (!initialized_ && !Initialize())
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
        if (!commandList)
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        CouplingConstants constants{};
        constants.activeParticleCount = particles.GetActiveParticleCount();
        constants.deltaTime = (std::max)(deltaTime, 0.0f);
        constants.blendBand = (std::max)(blendBand, 0.001f);
        constants.velocityCoupling = (std::max)(velocityCoupling, 0.0f);
        constants.surfacePoint = { 0.0f, sample.height, 0.0f };
        constants.surfaceAttraction = (std::max)(surfaceAttraction, 0.0f);
        constants.surfaceNormal = Vector3::NormalizeSafe(sample.normal, { 0.0f, 1.0f, 0.0f });
        constants.maxCorrection = (std::max)(maxCorrection, 0.0f);
        constants.surfaceVelocity = sample.velocity;

        const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(constants);
        if (!allocation.IsValid())
        {
            stats_.lastDispatchSucceeded = false;
            return false;
        }

        particles.Transition(commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        UAVManager::GetInstance()->PreDispatch();
        commandList->SetComputeRootSignature(rootSignature_.Get());
        commandList->SetPipelineState(pipelineState_.Get());
        commandList->SetComputeRootConstantBufferView(0, allocation.gpuAddress);
        commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(particles.GetUavIndex()));
        const uint32_t groupCount = (particles.GetActiveParticleCount() + 127u) / 128u;
        commandList->Dispatch(groupCount, 1, 1);
        particles.InsertUavBarrier(commandList); // Ocean連成後のSolverが更新済みVelocityを確実に参照できるようにする。

        ++stats_.dispatchCount;
        stats_.lastDispatchSucceeded = true;
        return true;
    }

    [[nodiscard]] const GpuProductionLiquidOceanCouplerStats& GetStats() const { return stats_; }

private:
    struct CouplingConstants
    {
        uint32_t activeParticleCount = 0;
        float deltaTime = 0.0f;
        float blendBand = 3.0f;
        float velocityCoupling = 3.0f;
        Vector3 surfacePoint{};
        float surfaceAttraction = 6.0f;
        Vector3 surfaceNormal{ 0.0f, 1.0f, 0.0f };
        float maxCorrection = 4.0f;
        Vector3 surfaceVelocity{};
        float padding = 0.0f;
    };
    static_assert(sizeof(CouplingConstants) == 64);

    bool CreateRootSignature()
    {
        D3D12_DESCRIPTOR_RANGE particleRange{};
        particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        particleRange.NumDescriptors = 1;
        particleRange.BaseShaderRegister = 0;
        particleRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor.ShaderRegister = 0;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &particleRange;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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

    bool CreatePipelineState()
    {
        const ShaderDescriptor descriptor{
            L"GpuLiquidOceanCouplingCS",
            L"Resources/Shaders/GpuFluid/Sph/Production/GpuSphOceanCoupling.CS.hlsl",
            L"main",
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
        return SUCCEEDED(dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelineState_)));
    }

    GpuProductionLiquidOceanCoupler() = default;

    DirectXCommon* dxCommon_ = nullptr;
    ComPtr<ID3D12RootSignature> rootSignature_{};
    ComPtr<ID3D12PipelineState> pipelineState_{};
    GpuProductionLiquidOceanCouplerStats stats_{};
    bool initialized_ = false;
};

} // namespace Ken4lowEngine
