#pragma once
#include "DX12Include.h"
#include <BlendModeType.h>
#include "Vector3.h"
#include "Quaternion.h"
#include <ModelData.h>
#include <TransformationMatrix.h>
#include "LightManager.h"
#include "Material.h"

#include <array>
#include <string>
#include <vector>
#include <numbers>
#include <map>

namespace Ken4lowEngine
{

class DirectXCommon;

class AnimationPipelineBuilder
{
public:
	static AnimationPipelineBuilder* GetInstance();
	void Initialize(DirectXCommon* dxCommon);
	void Finalize();

	void SetRenderSetting(MaterialCullMode cullMode = MaterialCullMode::Back);
	void SetComputeSetting();

	ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }
	ID3D12PipelineState* GetPipelineState(MaterialCullMode cullMode = MaterialCullMode::Back) const;
	ID3D12RootSignature* GetComputeRootSignature() const { return computeRootSignature_.Get(); }
	ID3D12PipelineState* GetComputePipelineState() const { return computePipelineState_.Get(); }

private:
	void CreateRootSignature();
	void CreatePSO();
	void CreateComputeRootSignature();
	void CreateComputePSO();

private:
	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> graphicsPipelineState;
	ComPtr<ID3D12PipelineState> graphicsPipelineStateFront_;
	ComPtr<ID3D12PipelineState> graphicsPipelineStateTwoSided_;

	ComPtr<ID3D12RootSignature> computeRootSignature_;
	ComPtr<ID3D12PipelineState> computePipelineState_;

	BlendMode blendMode_ = BlendMode::kBlendModeNone;
};

} // namespace Ken4lowEngine
