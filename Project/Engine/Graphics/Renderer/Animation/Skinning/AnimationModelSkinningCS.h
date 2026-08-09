#pragma once
#include "DX12Include.h"
#include <cstdint>

#include <SkinCluster.h> // SkinningInformationForGPU / SkinCluster

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// -------------------------------------------------------------
	/// Compute Shader によるスキニング処理（CB確保/Dispatch）を担当
	/// -------------------------------------------------------------
	class AnimationModelSkinningCS
	{
	public:
		void Initialize(DirectXCommon* dxCommon, uint32_t initialVertexCount, bool isSkinning);
		void Reset();

		bool IsInitialized() const { return mapped_ != nullptr; }
		bool IsSkinningModel() const { return mapped_ && mapped_->isSkinning; }

		void SetVertexCount(uint32_t v) { if (mapped_) { mapped_->numVertices = v; } }
		uint32_t GetVertexCount() const { return mapped_ ? mapped_->numVertices : 0u; }

		// ランタイムで Compute を走らせるか（Freeze用）
		void SetRuntimeEnabled(bool v) { runtimeEnabled_ = v; }
		bool IsRuntimeEnabled() const { return runtimeEnabled_; }

		/// Compute スキニングを実行（リソースステート遷移もここで実施）
		void Dispatch(
			DirectXCommon* dxCommon,
			SkinCluster* skinCluster,
			D3D12_GPU_DESCRIPTOR_HANDLE inputVerticesSrv,
			D3D12_GPU_DESCRIPTOR_HANDLE influenceSrv,
			D3D12_GPU_DESCRIPTOR_HANDLE outputUav,
			uint32_t vertexCount,
			ID3D12Resource* skinnedVB,
			D3D12_RESOURCE_STATES& skinnedState,
			D3D12_GPU_DESCRIPTOR_HANDLE externalPaletteSrv = {});

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> csCB_;                     // b0
		SkinningInformationForGPU* mapped_ = nullptr;                      // b0 map
		bool runtimeEnabled_ = true;
	};
}
