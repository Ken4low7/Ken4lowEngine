#include "GpuParticleComputePipeline.h"
#include <DirectXCommon.h>
#include <LogString.h>

#include "GpuParticleShaderManifest.h"
#include "ShaderCompiler.h"
#include <cassert>

namespace Ken4lowEngine
{

	void GpuParticleComputePipeline::Initialize()
	{
		dxCommon_ = DirectXCommon::GetInstance();

		CreateComputeRootSignature();
		CreateComputePSO();
		CreateEmitComputePSO();
		CreateUpdateComputePSO();

		computeRootSignature_->SetName(L"GpuParticleComputePipeline_RootSignature");
		computePipelineState_->SetName(L"GpuParticleComputePipeline_Sim_PSO");
		emitComputePipelineState_->SetName(L"GpuParticleComputePipeline_Emit_PSO");
		updateComputePipelineState_->SetName(L"GpuParticleComputePipeline_Update_PSO");
	}

	void GpuParticleComputePipeline::CreateComputeRootSignature()
	{
		// b0: sim定数 / UAV table / b1: emitter / b2: perframe
		D3D12_ROOT_PARAMETER params[4]{};

		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_DESCRIPTOR_RANGE uavRange{};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 5;
		uavRange.BaseShaderRegister = 0;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		params[1].DescriptorTable.NumDescriptorRanges = 1;
		params[1].DescriptorTable.pDescriptorRanges = &uavRange;
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[2].Descriptor.ShaderRegister = 1;
		params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[3].Descriptor.ShaderRegister = 2;
		params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		desc.pParameters = params;
		desc.NumParameters = _countof(params);

		ComPtr<ID3DBlob> sig, err;
		HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
		if (FAILED(hr))
		{
			Log(err ? reinterpret_cast<char*>(err->GetBufferPointer()) : "Compute RS serialize failed");
			assert(false);
		}

		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			sig->GetBufferPointer(),
			sig->GetBufferSize(),
			IID_PPV_ARGS(&computeRootSignature_));
		assert(SUCCEEDED(hr));
	}

	void GpuParticleComputePipeline::CreateComputePSO()
	{
		const ShaderDescriptor& csDesc =
			GpuParticleShaderManifest::GetCompute(GpuParticleComputeShaderId::SimCS);

		assert(csDesc.stage == ShaderStage::Compute);
		assert(csDesc.rootSignature == RootSignatureType::Compute);

		ComPtr<IDxcBlob> cs = ShaderCompiler::CompileShader(
			csDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(cs != nullptr);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = computeRootSignature_.Get();
		desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

		HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
			&desc,
			IID_PPV_ARGS(&computePipelineState_));
		assert(SUCCEEDED(hr));
	}

	void GpuParticleComputePipeline::CreateEmitComputePSO()
	{
		const ShaderDescriptor& csDesc =
			GpuParticleShaderManifest::GetCompute(GpuParticleComputeShaderId::EmitCS);

		assert(csDesc.stage == ShaderStage::Compute);
		assert(csDesc.rootSignature == RootSignatureType::Compute);

		ComPtr<IDxcBlob> cs = ShaderCompiler::CompileShader(
			csDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(cs != nullptr);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = computeRootSignature_.Get();
		desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

		HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
			&desc,
			IID_PPV_ARGS(&emitComputePipelineState_));
		assert(SUCCEEDED(hr));
	}

	void GpuParticleComputePipeline::CreateUpdateComputePSO()
	{
		const ShaderDescriptor& csDesc =
			GpuParticleShaderManifest::GetCompute(GpuParticleComputeShaderId::UpdateCS);

		assert(csDesc.stage == ShaderStage::Compute);
		assert(csDesc.rootSignature == RootSignatureType::Compute);

		ComPtr<IDxcBlob> cs = ShaderCompiler::CompileShader(
			csDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(cs != nullptr);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = computeRootSignature_.Get();
		desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

		HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
			&desc,
			IID_PPV_ARGS(&updateComputePipelineState_));
		assert(SUCCEEDED(hr));
	}

} // namespace Ken4lowEngine