#include "PlayerHealthPostEffect.h"
#include <DirectXCommon.h>
#include <PostEffectPipelineBuilder.h>
#include <ResourceManager.h>
#include <UAVManager.h>
#include <PostEffectManager.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

void PlayerHealthPostEffect::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder)
{
	dxCommon_ = dxCommon;

	computeRootSignature_ = builder->CreateComputeRootSignature();
	computePipelineState_ = builder->CreateComputePipeline(PostEffectComputeShaderId::PlayerHealthCS, computeRootSignature_.Get());

	constantBuffer_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(EffectSetting));
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&setting_));
	WriteToConstantBuffer();

	constantBuffer_->SetName(L"PlayerHealthPostEffect::ConstantBuffer");
	computeRootSignature_->SetName(L"PlayerHealthPostEffect::ComputeRootSignature");
	computePipelineState_->SetName(L"PlayerHealthPostEffect::ComputePipelineState");
}

void PlayerHealthPostEffect::Finalize()
{
	if (constantBuffer_ && setting_)
	{
		constantBuffer_->Unmap(0, nullptr);
		setting_ = nullptr;
	}

	constantBuffer_.Reset();
	computePipelineState_.Reset();
	computeRootSignature_.Reset();
	dxCommon_ = nullptr;
}

void PlayerHealthPostEffect::Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex)
{
	(void)dsvIndex;

	WriteToConstantBuffer();

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(computePipelineState_.Get());
	commandList->SetComputeRootDescriptorTable(0, UAVManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
	commandList->SetComputeRootDescriptorTable(1, UAVManager::GetInstance()->GetGPUDescriptorHandle(uavIndex));
	commandList->SetComputeRootConstantBufferView(2, constantBuffer_->GetGPUVirtualAddress());

	const uint32_t threadGroupSizeX = 8;
	const uint32_t threadGroupSizeY = 8;
	// Compute Dispatch範囲を現在のGameViewportRenderTargetサイズに合わせる。
	const uint32_t width = PostEffectManager::GetInstance()->GetGameRenderTargetWidth();
	const uint32_t height = PostEffectManager::GetInstance()->GetGameRenderTargetHeight();
	const uint32_t groupCountX = (width + threadGroupSizeX - 1) / threadGroupSizeX;
	const uint32_t groupCountY = (height + threadGroupSizeY - 1) / threadGroupSizeY;

	commandList->Dispatch(groupCountX, groupCountY, 1);
}

void PlayerHealthPostEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Text("PlayerHealthPostEffect は PlayerHealthPostEffectController から制御します");
	ImGui::Text("Vignette %.2f / Flash %.2f", parameters_.lowHealthVignetteIntensity, parameters_.damageFlashIntensity);
#endif // USE_IMGUI
}

void PlayerHealthPostEffect::SetParameters(const Parameters& parameters)
{
	parameters_ = parameters;
	WriteToConstantBuffer();
}

void PlayerHealthPostEffect::SetElapsedTime(float elapsedTime)
{
	elapsedTime_ = elapsedTime;
	WriteToConstantBuffer();
}

void PlayerHealthPostEffect::WriteToConstantBuffer()
{
	if (!setting_)
	{
		return;
	}

	setting_->vignetteColor = parameters_.vignetteColor;
	setting_->lowHealthVignetteIntensity = parameters_.lowHealthVignetteIntensity;
	setting_->damageFlashIntensity = parameters_.damageFlashIntensity;
	setting_->desaturation = parameters_.desaturation;
	setting_->darkenIntensity = parameters_.darkenIntensity;
	setting_->pulseSpeed = parameters_.pulseSpeed;
	setting_->pulseIntensity = parameters_.pulseIntensity;
	setting_->elapsedTime = elapsedTime_;
}

} // namespace Ken4lowEngine
