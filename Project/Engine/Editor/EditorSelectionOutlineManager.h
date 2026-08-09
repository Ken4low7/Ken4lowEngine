#pragma once

#include <DX12Include.h>
#include <PipelineCommon.h>

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	class BaseScene;
	class DirectXCommon;
	struct EditorObjectInfo;

	/// <summary>
	/// Scene全体の深度付きSelection Maskから、選択Objectだけのシルエット輪郭を生成します。
	/// </summary>
	class EditorSelectionOutlineManager
	{
	public:
		static EditorSelectionOutlineManager* GetInstance();

		void Initialize();
		void Finalize();
		void Render(BaseScene* scene);

		bool HasVisibleOutline() const { return hasVisibleOutline_; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetOutlineSrvHandleGPU() const;

	private:
		EditorSelectionOutlineManager() = default;
		~EditorSelectionOutlineManager() = default;
		EditorSelectionOutlineManager(const EditorSelectionOutlineManager&) = delete;
		EditorSelectionOutlineManager& operator=(const EditorSelectionOutlineManager&) = delete;

		void CreateMaskResources();
		void CreateOutlineResources();
		void CreateOutlinePipeline();
		bool IsSelectedDrawable(
			const EditorObjectInfo& object,
			const EditorObjectInfo& selected,
			const std::vector<EditorObjectInfo>& objects) const;

		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> selectionMask_;
		Microsoft::WRL::ComPtr<ID3D12Resource> selectionDepth_;
		Microsoft::WRL::ComPtr<ID3D12Resource> outlineTexture_;
		PipelineBundle outlinePipeline_{};
		uint32_t maskRtvIndex_ = UINT32_MAX;
		uint32_t maskSrvIndex_ = UINT32_MAX;
		uint32_t depthDsvIndex_ = UINT32_MAX;
		uint32_t outlineRtvIndex_ = UINT32_MAX;
		uint32_t outlineSrvIndex_ = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE maskRtvHandle_{};
		D3D12_CPU_DESCRIPTOR_HANDLE depthDsvHandle_{};
		D3D12_CPU_DESCRIPTOR_HANDLE outlineRtvHandle_{};
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
		bool initialized_ = false;
		bool hasVisibleOutline_ = false;
	};
} // namespace Ken4lowEngine
