#pragma once

#include "EditorContext.h"
#include "EditorObjectInfo.h"
#include "EditorViewportController.h"

#include <BaseScene.h>
#include <DSVManager.h>
#include <DirectXCommon.h>
#include <GameViewportConstants.h>
#include <ObjectIdPipeline.h>
#include <RTVManager.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// R32_UINTのObject-ID RenderTargetへEditor Componentを描画し、クリックした1PixelだけをReadbackします。
	/// Instancing Componentは連続ID範囲を予約し、1Drawのまま各Instanceを識別します。
	/// </summary>
	class EditorGpuPickingManager
	{
	public:
		struct ExecuteResult
		{
			bool executed = false;
			bool hit = false;
			std::string message;
		};

		static EditorGpuPickingManager* GetInstance()
		{
			static EditorGpuPickingManager instance;
			return &instance;
		}

		void Initialize();
		void Finalize();
		void RequestPick(uint32_t pixelX, uint32_t pixelY, EditorViewportSelectionMode selectionMode);
		bool HasPendingRequest() const { return pendingRequest_.pending; }
		ExecuteResult Execute(BaseScene* scene);

	private:
		struct PickRequest
		{
			uint32_t pixelX = 0;
			uint32_t pixelY = 0;
			EditorViewportSelectionMode selectionMode = EditorViewportSelectionMode::Actor;
			bool pending = false;
		};

		EditorGpuPickingManager() = default;
		~EditorGpuPickingManager() = default;
		EditorGpuPickingManager(const EditorGpuPickingManager&) = delete;
		EditorGpuPickingManager& operator=(const EditorGpuPickingManager&) = delete;

		void CreateObjectIdRenderTarget();
		void CreateDepthBuffer();
		void CreateReadbackBuffer();
		void CopyRequestedPixel(ID3D12GraphicsCommandList* commandList, uint32_t pixelX, uint32_t pixelY);
		uint32_t ReadbackObjectId() const;
		static const EditorObjectInfo* FindActorAncestor(
			const std::vector<EditorObjectInfo>& objects,
			const EditorObjectInfo& hitObject);

		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> objectIdBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuffer_;
		uint32_t rtvIndex_ = UINT32_MAX;
		uint32_t dsvIndex_ = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};
		D3D12_VIEWPORT viewport_{};
		D3D12_RECT scissorRect_{};
		PickRequest pendingRequest_{};
		bool initialized_ = false;
	};
} // namespace Ken4lowEngine
