#pragma once

#include "../Resource/GpuSphParticleBuffer.h"

#include <CameraManager.h>
#include <DirectXCommon.h>
#include <LogString.h>
#include <PostEffectManager.h>
#include <RTVManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>
#include <ShaderManifestTypes.h>
#include <Vector4.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace Ken4lowEngine
{

struct GpuSphScreenSpaceRenderSettings
{
	bool enabled = true;
	float particleRadius = 0.075f;
	float blurDepthFalloff = 28.0f;
	float absorption = 2.8f;
	float refractionStrength = 0.012f;
	float fresnelPower = 5.0f;
	float thicknessScale = 5.0f;
	Vector4 shallowColor{ 0.10f, 0.52f, 0.72f, 1.0f };
	Vector4 deepColor{ 0.015f, 0.12f, 0.26f, 1.0f };
};

struct GpuSphScreenSpaceRenderStats
{
	uint64_t frameCount = 0;
	uint64_t particleDepthDrawCount = 0;
	uint64_t thicknessDrawCount = 0;
	uint64_t blurDrawCount = 0;
	uint64_t compositeDrawCount = 0;
	uint32_t lastParticleCount = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	bool initialized = false;
	bool lastDrawSucceeded = true;
};

/// W8: SPH粒子をDepth/Thicknessへ投影し、Bilateral BlurとScreen-space Compositeで連続した水面へ変換する。
class GpuSphScreenSpaceFluidRenderer
{
public:
	static GpuSphScreenSpaceFluidRenderer* GetInstance()
	{
		static GpuSphScreenSpaceFluidRenderer instance;
		return &instance;
	}

	bool Draw(GpuSphParticleBuffer& particleBuffer)
	{
		stats_.lastDrawSucceeded = true;
		if (!settings_.enabled || !particleBuffer.IsInitialized() || particleBuffer.GetActiveParticleCount() == 0)
		{
			return true;
		}

		PostEffectManager* postEffect = PostEffectManager::GetInstance();
		const uint32_t width = postEffect->GetGameRenderTargetWidth();
		const uint32_t height = postEffect->GetGameRenderTargetHeight();
		if ((!initialized_ && !Initialize(width, height)) || !EnsureSize(width, height))
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		CameraManager* cameraManager = CameraManager::GetInstance();
		if (cameraManager == nullptr || dxCommon_ == nullptr || dxCommon_->GetCommandManager() == nullptr)
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		if (commandList == nullptr || !CaptureSceneColor(commandList))
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		particleBuffer.Transition(commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		SRVManager::GetInstance()->PreDraw();

		GpuSphScreenSpaceConstants baseConstants = BuildConstants(*cameraManager, width, height);
		const auto particleConstants = dxCommon_->GetFrameUploadArena().AllocateConstant(baseConstants);
		if (!particleConstants.IsValid())
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		if (!DrawParticleDepth(commandList, particleBuffer, particleConstants.gpuAddress) ||
			!DrawThickness(commandList, particleBuffer, particleConstants.gpuAddress))
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		GpuSphScreenSpaceConstants horizontalConstants = baseConstants;
		horizontalConstants.blurDirection = { 1.0f, 0.0f, 0.0f, 0.0f };
		const auto horizontalAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(horizontalConstants);
		GpuSphScreenSpaceConstants verticalConstants = baseConstants;
		verticalConstants.blurDirection = { 0.0f, 1.0f, 0.0f, 0.0f };
		const auto verticalAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(verticalConstants);
		if (!horizontalAllocation.IsValid() || !verticalAllocation.IsValid() ||
			!DrawBlur(commandList, depthRaw_, depthPing_, horizontalAllocation.gpuAddress) ||
			!DrawBlur(commandList, depthPing_, depthSmooth_, verticalAllocation.gpuAddress))
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		if (!DrawComposite(commandList, particleConstants.gpuAddress))
		{
			stats_.lastDrawSucceeded = false;
			return false;
		}

		++stats_.frameCount;
		stats_.lastParticleCount = particleBuffer.GetActiveParticleCount();
		stats_.width = width_;
		stats_.height = height_;
		stats_.initialized = true;
		return true;
	}

	void Finalize()
	{
		ReleaseTarget(depthRaw_);
		ReleaseTarget(depthPing_);
		ReleaseTarget(depthSmooth_);
		ReleaseTarget(thickness_);
		ReleaseSceneCopy();
		particleDepthPso_.Reset();
		thicknessPso_.Reset();
		blurPso_.Reset();
		compositePso_.Reset();
		particleRootSignature_.Reset();
		fullscreenRootSignature_.Reset();
		dxCommon_ = nullptr;
		width_ = 0;
		height_ = 0;
		initialized_ = false;
		stats_ = {};
	}

	GpuSphScreenSpaceRenderSettings& GetEditableSettings() { return settings_; }
	const GpuSphScreenSpaceRenderSettings& GetSettings() const { return settings_; }
	const GpuSphScreenSpaceRenderStats& GetStats() const { return stats_; }
	bool IsInitialized() const { return initialized_; }

private:
	struct ScreenTarget
	{
		ComPtr<ID3D12Resource> resource{};
		uint32_t rtvIndex = UINT32_MAX;
		uint32_t srvIndex = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
		D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};

	struct GpuSphScreenSpaceConstants
	{
		Matrix4x4 view = Matrix4x4::MakeIdentity();
		Matrix4x4 projection = Matrix4x4::MakeIdentity();
		Vector4 shallowColor{};
		Vector4 deepColor{};
		Vector4 screenRadiusFar{}; // width, height, radius, farClip
		Vector4 cameraParams{}; // nearClip, fovY, aspect, blurDepthFalloff
		Vector4 opticalParams{}; // absorption, refractionStrength, fresnelPower, thicknessScale
		Vector4 blurDirection{};
	};
	static_assert(sizeof(GpuSphScreenSpaceConstants) == 224);
	static constexpr float kDepthClearValue = 1000000.0f;
	static constexpr float kThicknessClearValue = 0.0f;

	GpuSphScreenSpaceFluidRenderer() = default;
	~GpuSphScreenSpaceFluidRenderer() = default;
	GpuSphScreenSpaceFluidRenderer(const GpuSphScreenSpaceFluidRenderer&) = delete;
	GpuSphScreenSpaceFluidRenderer& operator=(const GpuSphScreenSpaceFluidRenderer&) = delete;

	bool Initialize(uint32_t width, uint32_t height)
	{
		dxCommon_ = DirectXCommon::GetInstance();
		if (dxCommon_ == nullptr || dxCommon_->GetDevice() == nullptr || width == 0 || height == 0)
		{
			return false;
		}

		if (!CreateParticleRootSignature() || !CreateFullscreenRootSignature() || !CreatePipelineStates() || !CreateTargets(width, height))
		{
			Finalize();
			return false;
		}

		particleRootSignature_->SetName(L"GpuSph.W8.Particle.RootSignature");
		fullscreenRootSignature_->SetName(L"GpuSph.W8.Fullscreen.RootSignature");
		particleDepthPso_->SetName(L"GpuSph.W8.ParticleDepth.PSO");
		thicknessPso_->SetName(L"GpuSph.W8.Thickness.PSO");
		blurPso_->SetName(L"GpuSph.W8.BilateralBlur.PSO");
		compositePso_->SetName(L"GpuSph.W8.Composite.PSO");
		initialized_ = true;
		stats_.initialized = true;
		return true;
	}

	bool EnsureSize(uint32_t width, uint32_t height)
	{
		if (width_ == width && height_ == height)
		{
			return true;
		}
		if (dxCommon_ && dxCommon_->GetCommandManager())
		{
			dxCommon_->GetCommandManager()->ExecuteAndWait(); // Resize時だけ旧Screen TextureのGPU参照完了を待つ。
		}
		ReleaseTarget(depthRaw_);
		ReleaseTarget(depthPing_);
		ReleaseTarget(depthSmooth_);
		ReleaseTarget(thickness_);
		ReleaseSceneCopy();
		return CreateTargets(width, height);
	}

	bool CreateTargets(uint32_t width, uint32_t height)
	{
		width_ = width;
		height_ = height;
		return CreateTarget(depthRaw_, DXGI_FORMAT_R32_FLOAT, L"GpuSph.W8.DepthRaw") &&
			CreateTarget(depthPing_, DXGI_FORMAT_R32_FLOAT, L"GpuSph.W8.DepthPing") &&
			CreateTarget(depthSmooth_, DXGI_FORMAT_R32_FLOAT, L"GpuSph.W8.DepthSmooth") &&
			CreateTarget(thickness_, DXGI_FORMAT_R16_FLOAT, L"GpuSph.W8.Thickness") &&
			CreateSceneCopy();
	}

	bool CreateTarget(ScreenTarget& target, DXGI_FORMAT format, const wchar_t* name)
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width_;
		desc.Height = height_;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = format;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		heap.CreationNodeMask = 1;
		heap.VisibleNodeMask = 1;

		const float optimizedScalar = format == DXGI_FORMAT_R16_FLOAT ? kThicknessClearValue : kDepthClearValue;
		D3D12_CLEAR_VALUE optimizedClear{};
		optimizedClear.Format = format;
		optimizedClear.Color[0] = optimizedScalar;
		optimizedClear.Color[1] = optimizedScalar;
		optimizedClear.Color[2] = optimizedScalar;
		optimizedClear.Color[3] = optimizedScalar;
		// W8専用RTはDebug LayerのClear契約とFast Clearの両方を満たす値をResource作成時に固定する。
		if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			&optimizedClear,
			IID_PPV_ARGS(&target.resource))))
		{
			return false;
		}
		target.resource->SetName(name);
		target.format = format;
		target.state = D3D12_RESOURCE_STATE_COMMON;

		try
		{
			target.rtvIndex = RTVManager::GetInstance()->Allocate();
			target.srvIndex = SRVManager::GetInstance()->Allocate();
		}
		catch (...)
		{
			ReleaseTarget(target);
			return false;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		target.rtvHandle = RTVManager::GetInstance()->GetCPUDescriptorHandle(target.rtvIndex);
		dxCommon_->GetDevice()->CreateRenderTargetView(target.resource.Get(), &rtvDesc, target.rtvHandle);
		SRVManager::GetInstance()->CreateSRVForTexture2D(target.srvIndex, target.resource.Get(), format, 1);
		return true;
	}

	bool CreateSceneCopy()
	{
		D3D12_RESOURCE_DESC desc{};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = width_;
		desc.Height = height_;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_DEFAULT;
		heap.CreationNodeMask = 1;
		heap.VisibleNodeMask = 1;
		if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(
			&heap,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&sceneCopy_))))
		{
			return false;
		}
		sceneCopy_->SetName(L"GpuSph.W8.SceneColorCopy");
		sceneCopyState_ = D3D12_RESOURCE_STATE_COMMON;
		try
		{
			sceneCopySrvIndex_ = SRVManager::GetInstance()->Allocate();
		}
		catch (...)
		{
			ReleaseSceneCopy();
			return false;
		}
		SRVManager::GetInstance()->CreateSRVForTexture2D(
			sceneCopySrvIndex_, sceneCopy_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
		return true;
	}

	void ReleaseTarget(ScreenTarget& target)
	{
		if (target.rtvIndex != UINT32_MAX) RTVManager::GetInstance()->Free(target.rtvIndex);
		if (target.srvIndex != UINT32_MAX) SRVManager::GetInstance()->Free(target.srvIndex);
		target = {};
	}

	void ReleaseSceneCopy()
	{
		if (sceneCopySrvIndex_ != UINT32_MAX) SRVManager::GetInstance()->Free(sceneCopySrvIndex_);
		sceneCopySrvIndex_ = UINT32_MAX;
		sceneCopy_.Reset();
		sceneCopyState_ = D3D12_RESOURCE_STATE_COMMON;
	}

	void TransitionTarget(ID3D12GraphicsCommandList* commandList, ScreenTarget& target, D3D12_RESOURCE_STATES nextState)
	{
		if (!commandList || !target.resource || target.state == nextState) return;
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = target.resource.Get();
		barrier.Transition.StateBefore = target.state;
		barrier.Transition.StateAfter = nextState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		target.state = nextState;
	}

	void TransitionSceneCopy(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES nextState)
	{
		if (!commandList || !sceneCopy_ || sceneCopyState_ == nextState) return;
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = sceneCopy_.Get();
		barrier.Transition.StateBefore = sceneCopyState_;
		barrier.Transition.StateAfter = nextState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		sceneCopyState_ = nextState;
	}

	bool CaptureSceneColor(ID3D12GraphicsCommandList* commandList)
	{
		PostEffectManager* postEffect = PostEffectManager::GetInstance();
		ID3D12Resource* sceneResource = postEffect->GetGameRenderTargetResource();
		if (!sceneResource || !sceneCopy_) return false;

		D3D12_RESOURCE_STATES sceneState = postEffect->GetGameRenderTargetState();
		if (sceneState != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			dxCommon_->ResourceTransition(sceneResource, sceneState, D3D12_RESOURCE_STATE_COPY_SOURCE);
			postEffect->SetGameRenderTargetState(D3D12_RESOURCE_STATE_COPY_SOURCE);
		}
		TransitionSceneCopy(commandList, D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->CopyResource(sceneCopy_.Get(), sceneResource);
		TransitionSceneCopy(commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		dxCommon_->ResourceTransition(sceneResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		postEffect->SetGameRenderTargetState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		return true;
	}

	GpuSphScreenSpaceConstants BuildConstants(
		const CameraManager& cameraManager,
		uint32_t width,
		uint32_t height) const
	{
		GpuSphScreenSpaceConstants constants{};
		constants.view = cameraManager.GetActiveViewMatrix();
		constants.projection = cameraManager.GetActiveProjectionMatrix();
		constants.shallowColor = settings_.shallowColor;
		constants.deepColor = settings_.deepColor;
		constants.screenRadiusFar = {
			static_cast<float>(width),
			static_cast<float>(height),
			(std::max)(settings_.particleRadius, 0.001f),
			(std::max)(cameraManager.GetActiveFarClip(), 1.0f) };
		constants.cameraParams = {
			(std::max)(cameraManager.GetActiveNearClip(), 0.001f),
			cameraManager.GetActiveFovY(),
			(std::max)(cameraManager.GetActiveAspectRatio(), 0.001f),
			(std::max)(settings_.blurDepthFalloff, 0.001f) };
		constants.opticalParams = {
			(std::max)(settings_.absorption, 0.0f),
			settings_.refractionStrength,
			(std::max)(settings_.fresnelPower, 1.0f),
			(std::max)(settings_.thicknessScale, 0.0f) };
		return constants;
	}

	bool DrawParticleDepth(
		ID3D12GraphicsCommandList* commandList,
		GpuSphParticleBuffer& particleBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS constants)
	{
		TransitionTarget(commandList, depthRaw_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const float clearValue[4] = { kDepthClearValue, kDepthClearValue, kDepthClearValue, kDepthClearValue };
		commandList->ClearRenderTargetView(depthRaw_.rtvHandle, clearValue, 0, nullptr);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = PostEffectManager::GetInstance()->GetSceneDsvHandle();
		commandList->OMSetRenderTargets(1, &depthRaw_.rtvHandle, false, dsv.ptr != 0 ? &dsv : nullptr);
		SetViewport(commandList);
		commandList->SetGraphicsRootSignature(particleRootSignature_.Get());
		commandList->SetPipelineState(particleDepthPso_.Get());
		commandList->SetGraphicsRootConstantBufferView(0, constants);
		commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetGPUDescriptorHandle(particleBuffer.GetSrvIndex()));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(6, particleBuffer.GetActiveParticleCount(), 0, 0);
		commandList->OMSetRenderTargets(0, nullptr, false, nullptr);
		TransitionTarget(commandList, depthRaw_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		++stats_.particleDepthDrawCount;
		return true;
	}

	bool DrawThickness(
		ID3D12GraphicsCommandList* commandList,
		GpuSphParticleBuffer& particleBuffer,
		D3D12_GPU_VIRTUAL_ADDRESS constants)
	{
		TransitionTarget(commandList, thickness_, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const float clearValue[4] = { kThicknessClearValue, kThicknessClearValue, kThicknessClearValue, kThicknessClearValue };
		commandList->ClearRenderTargetView(thickness_.rtvHandle, clearValue, 0, nullptr);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = PostEffectManager::GetInstance()->GetSceneDsvHandle();
		commandList->OMSetRenderTargets(1, &thickness_.rtvHandle, false, dsv.ptr != 0 ? &dsv : nullptr);
		SetViewport(commandList);
		commandList->SetGraphicsRootSignature(particleRootSignature_.Get());
		commandList->SetPipelineState(thicknessPso_.Get());
		commandList->SetGraphicsRootConstantBufferView(0, constants);
		commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetGPUDescriptorHandle(particleBuffer.GetSrvIndex()));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(6, particleBuffer.GetActiveParticleCount(), 0, 0);
		commandList->OMSetRenderTargets(0, nullptr, false, nullptr);
		TransitionTarget(commandList, thickness_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		++stats_.thicknessDrawCount;
		return true;
	}

	bool DrawBlur(
		ID3D12GraphicsCommandList* commandList,
		ScreenTarget& source,
		ScreenTarget& destination,
		D3D12_GPU_VIRTUAL_ADDRESS constants)
	{
		TransitionTarget(commandList, source, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		TransitionTarget(commandList, destination, D3D12_RESOURCE_STATE_RENDER_TARGET);
		const float clearValue[4] = { kDepthClearValue, kDepthClearValue, kDepthClearValue, kDepthClearValue };
		commandList->ClearRenderTargetView(destination.rtvHandle, clearValue, 0, nullptr);
		commandList->OMSetRenderTargets(1, &destination.rtvHandle, false, nullptr);
		SetViewport(commandList);
		commandList->SetGraphicsRootSignature(fullscreenRootSignature_.Get());
		commandList->SetPipelineState(blurPso_.Get());
		commandList->SetGraphicsRootConstantBufferView(0, constants);
		commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetGPUDescriptorHandle(source.srvIndex));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
		commandList->OMSetRenderTargets(0, nullptr, false, nullptr);
		TransitionTarget(commandList, destination, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		++stats_.blurDrawCount;
		return true;
	}

	bool DrawComposite(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS constants)
	{
		if (!depthSmooth_.resource || !thickness_.resource || !sceneCopy_ || sceneCopySrvIndex_ == UINT32_MAX)
		{
			return false;
		}
		SRVManager::GetInstance()->PreDraw();
		SetViewport(commandList);
		commandList->SetGraphicsRootSignature(fullscreenRootSignature_.Get());
		commandList->SetPipelineState(compositePso_.Get());
		commandList->SetGraphicsRootConstantBufferView(0, constants);
		commandList->SetGraphicsRootDescriptorTable(1, SRVManager::GetInstance()->GetGPUDescriptorHandle(depthSmooth_.srvIndex));
		commandList->SetGraphicsRootDescriptorTable(2, SRVManager::GetInstance()->GetGPUDescriptorHandle(thickness_.srvIndex));
		commandList->SetGraphicsRootDescriptorTable(3, SRVManager::GetInstance()->GetGPUDescriptorHandle(sceneCopySrvIndex_));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
		++stats_.compositeDrawCount;
		return true;
	}

	void SetViewport(ID3D12GraphicsCommandList* commandList) const
	{
		D3D12_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f };
		D3D12_RECT scissor{ 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissor);
	}

	bool CreateParticleRootSignature()
	{
		D3D12_DESCRIPTOR_RANGE particleRange{};
		particleRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
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
		params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return SerializeRootSignature(desc, particleRootSignature_);
	}

	bool CreateFullscreenRootSignature()
	{
		D3D12_DESCRIPTOR_RANGE ranges[3]{};
		D3D12_ROOT_PARAMETER params[4]{};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		for (uint32_t index = 0; index < 3; ++index)
		{
			ranges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[index].NumDescriptors = 1;
			ranges[index].BaseShaderRegister = index;
			ranges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			params[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[index + 1].DescriptorTable.NumDescriptorRanges = 1;
			params[index + 1].DescriptorTable.pDescriptorRanges = &ranges[index];
			params[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = _countof(params);
		desc.pParameters = params;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &sampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return SerializeRootSignature(desc, fullscreenRootSignature_);
	}

	bool SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>& output)
	{
		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
		{
			if (error) Log(std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()));
			return false;
		}
		return SUCCEEDED(dxCommon_->GetDevice()->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&output)));
	}

	ShaderDescriptor MakeShader(
		const wchar_t* debugName,
		const wchar_t* path,
		const wchar_t* entry,
		const wchar_t* profile,
		ShaderStage stage) const
	{
		return { debugName, path, entry, profile, stage, RootSignatureType::GpuFluid };
	}

	bool CreatePipelineStates()
	{
		const auto particleVsDesc = MakeShader(L"GpuSphW8ParticleVS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceParticle.VS.hlsl", L"main", L"vs_6_0", ShaderStage::Vertex);
		const auto depthPsDesc = MakeShader(L"GpuSphW8DepthPS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceDepth.PS.hlsl", L"main", L"ps_6_0", ShaderStage::Pixel);
		const auto thicknessPsDesc = MakeShader(L"GpuSphW8ThicknessPS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceThickness.PS.hlsl", L"main", L"ps_6_0", ShaderStage::Pixel);
		const auto fullscreenVsDesc = MakeShader(L"GpuSphW8FullscreenVS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceFullscreen.VS.hlsl", L"main", L"vs_6_0", ShaderStage::Vertex);
		const auto blurPsDesc = MakeShader(L"GpuSphW8BlurPS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceBlur.PS.hlsl", L"main", L"ps_6_0", ShaderStage::Pixel);
		const auto compositePsDesc = MakeShader(L"GpuSphW8CompositePS", L"Resources/Shaders/GpuFluid/Sph/ScreenSpace/GpuSphScreenSpaceComposite.PS.hlsl", L"main", L"ps_6_0", ShaderStage::Pixel);

		ComPtr<IDxcBlob> particleVs = ShaderCompiler::CompileShader(particleVsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> depthPs = ShaderCompiler::CompileShader(depthPsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> thicknessPs = ShaderCompiler::CompileShader(thicknessPsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> fullscreenVs = ShaderCompiler::CompileShader(fullscreenVsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> blurPs = ShaderCompiler::CompileShader(blurPsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> compositePs = ShaderCompiler::CompileShader(compositePsDesc, dxCommon_->GetDXCCompilerManager());
		if (!particleVs || !depthPs || !thicknessPs || !fullscreenVs || !blurPs || !compositePs) return false;

		D3D12_RASTERIZER_DESC rasterizer{};
		rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizer.CullMode = D3D12_CULL_MODE_NONE;
		rasterizer.DepthClipEnable = TRUE;

		D3D12_DEPTH_STENCIL_DESC depthRead{};
		depthRead.DepthEnable = TRUE;
		depthRead.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		depthRead.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC particleDesc{};
		particleDesc.pRootSignature = particleRootSignature_.Get();
		particleDesc.InputLayout = { nullptr, 0 };
		particleDesc.VS = { particleVs->GetBufferPointer(), particleVs->GetBufferSize() };
		particleDesc.RasterizerState = rasterizer;
		particleDesc.DepthStencilState = depthRead;
		particleDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		particleDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		particleDesc.NumRenderTargets = 1;
		particleDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		particleDesc.SampleDesc.Count = 1;

		particleDesc.PS = { depthPs->GetBufferPointer(), depthPs->GetBufferSize() };
		particleDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
		auto& minBlend = particleDesc.BlendState.RenderTarget[0];
		minBlend.BlendEnable = TRUE;
		minBlend.SrcBlend = D3D12_BLEND_ONE;
		minBlend.DestBlend = D3D12_BLEND_ONE;
		minBlend.BlendOp = D3D12_BLEND_OP_MIN;
		minBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
		minBlend.DestBlendAlpha = D3D12_BLEND_ONE;
		minBlend.BlendOpAlpha = D3D12_BLEND_OP_MIN;
		minBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&particleDesc, IID_PPV_ARGS(&particleDepthPso_)))) return false;

		particleDesc.PS = { thicknessPs->GetBufferPointer(), thicknessPs->GetBufferSize() };
		particleDesc.RTVFormats[0] = DXGI_FORMAT_R16_FLOAT;
		particleDesc.BlendState = {};
		auto& addBlend = particleDesc.BlendState.RenderTarget[0];
		addBlend.BlendEnable = TRUE;
		addBlend.SrcBlend = D3D12_BLEND_ONE;
		addBlend.DestBlend = D3D12_BLEND_ONE;
		addBlend.BlendOp = D3D12_BLEND_OP_ADD;
		addBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
		addBlend.DestBlendAlpha = D3D12_BLEND_ONE;
		addBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		addBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&particleDesc, IID_PPV_ARGS(&thicknessPso_)))) return false;

		D3D12_DEPTH_STENCIL_DESC noDepth{};
		noDepth.DepthEnable = FALSE;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC fullscreenDesc{};
		fullscreenDesc.pRootSignature = fullscreenRootSignature_.Get();
		fullscreenDesc.InputLayout = { nullptr, 0 };
		fullscreenDesc.VS = { fullscreenVs->GetBufferPointer(), fullscreenVs->GetBufferSize() };
		fullscreenDesc.RasterizerState = rasterizer;
		fullscreenDesc.DepthStencilState = noDepth;
		fullscreenDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		fullscreenDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		fullscreenDesc.NumRenderTargets = 1;
		fullscreenDesc.SampleDesc.Count = 1;
		fullscreenDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		fullscreenDesc.PS = { blurPs->GetBufferPointer(), blurPs->GetBufferSize() };
		fullscreenDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
		if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&fullscreenDesc, IID_PPV_ARGS(&blurPso_)))) return false;

		fullscreenDesc.PS = { compositePs->GetBufferPointer(), compositePs->GetBufferSize() };
		fullscreenDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&fullscreenDesc, IID_PPV_ARGS(&compositePso_)))) return false;
		return true;
	}

private:
	DirectXCommon* dxCommon_ = nullptr;
	ScreenTarget depthRaw_{};
	ScreenTarget depthPing_{};
	ScreenTarget depthSmooth_{};
	ScreenTarget thickness_{};
	ComPtr<ID3D12Resource> sceneCopy_{};
	uint32_t sceneCopySrvIndex_ = UINT32_MAX;
	D3D12_RESOURCE_STATES sceneCopyState_ = D3D12_RESOURCE_STATE_COMMON;
	ComPtr<ID3D12RootSignature> particleRootSignature_{};
	ComPtr<ID3D12RootSignature> fullscreenRootSignature_{};
	ComPtr<ID3D12PipelineState> particleDepthPso_{};
	ComPtr<ID3D12PipelineState> thicknessPso_{};
	ComPtr<ID3D12PipelineState> blurPso_{};
	ComPtr<ID3D12PipelineState> compositePso_{};
	GpuSphScreenSpaceRenderSettings settings_{};
	GpuSphScreenSpaceRenderStats stats_{};
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
