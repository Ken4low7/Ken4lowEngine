#define NOMINMAX
#include "EditorSelectionOutlineManager.h"

#ifdef USE_IMGUI
#include "EditorContext.h"
#include "EditorObjectInfo.h"
#include "EditorPlayController.h"
#include "EditorViewportController.h"

#include <BaseScene.h>
#include <DSVManager.h>
#include <DirectXCommon.h>
#include <GameViewportConstants.h>
#include <ObjectIdPipeline.h>
#include <PipelineFactory.h>
#include <PipelineStatePresets.h>
#include <RTVManager.h>
#include <SRVManager.h>
#include <ShaderCompiler.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTexture(
			ID3D12Device* device,
			DXGI_FORMAT format,
			const float clearColor[4],
			D3D12_RESOURCE_STATES initialState)
		{
			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Width = GameViewportConstants::Width;
			desc.Height = GameViewportConstants::Height;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.Format = format;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_CLEAR_VALUE clearValue{};
			clearValue.Format = format;
			std::copy_n(clearColor, 4, clearValue.Color);

			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			const HRESULT hr = device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				initialState,
				&clearValue,
				IID_PPV_ARGS(&resource));
			if (FAILED(hr))
			{
				resource.Reset();
			}
			return resource;
		}
	}

	EditorSelectionOutlineManager* EditorSelectionOutlineManager::GetInstance()
	{
		static EditorSelectionOutlineManager instance;
		return &instance;
	}

	void EditorSelectionOutlineManager::Initialize()
	{
		if (initialized_)
		{
			return;
		}

		dxCommon_ = DirectXCommon::GetInstance();
		if (!dxCommon_ || !dxCommon_->GetDevice())
		{
			return;
		}

		CreateMaskResources();
		CreateOutlineResources();
		CreateOutlinePipeline();
		viewport_ = D3D12_VIEWPORT(
			0.0f,
			0.0f,
			static_cast<float>(GameViewportConstants::Width),
			static_cast<float>(GameViewportConstants::Height),
			0.0f,
			1.0f);
		scissorRect_ = { 0, 0, static_cast<LONG>(GameViewportConstants::Width), static_cast<LONG>(GameViewportConstants::Height) };
		initialized_ = selectionMask_ && selectionDepth_ && outlineTexture_ &&
			outlinePipeline_.rootSignature && outlinePipeline_.pipelineState;
		if (!initialized_)
		{
			Finalize(); // 部分生成のDescriptorを残さず、次回初期化を同じ状態から再試行できるようにする。
		}
	}

	void EditorSelectionOutlineManager::Finalize()
	{
		hasVisibleOutline_ = false;
		outlinePipeline_.Reset();
		selectionMask_.Reset();
		selectionDepth_.Reset();
		outlineTexture_.Reset();

		if (maskRtvIndex_ != UINT32_MAX) RTVManager::GetInstance()->Free(maskRtvIndex_);
		if (outlineRtvIndex_ != UINT32_MAX) RTVManager::GetInstance()->Free(outlineRtvIndex_);
		if (depthDsvIndex_ != UINT32_MAX) DSVManager::GetInstance()->Free(depthDsvIndex_);
		if (maskSrvIndex_ != UINT32_MAX) SRVManager::GetInstance()->Free(maskSrvIndex_);
		if (outlineSrvIndex_ != UINT32_MAX) SRVManager::GetInstance()->Free(outlineSrvIndex_);
		maskRtvIndex_ = outlineRtvIndex_ = depthDsvIndex_ = maskSrvIndex_ = outlineSrvIndex_ = UINT32_MAX;
		maskRtvHandle_ = {};
		depthDsvHandle_ = {};
		outlineRtvHandle_ = {};
		dxCommon_ = nullptr;
		initialized_ = false;
	}

	void EditorSelectionOutlineManager::Render(BaseScene* scene)
	{
		hasVisibleOutline_ = false;
		if (!initialized_)
		{
			Initialize();
		}
		EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
		if (!initialized_ || !scene || !selection.HasSelection() ||
			EditorPlayController::GetInstance()->IsPlaying() ||
			!EditorViewportController::GetInstance()->IsEditorDisplay())
		{
			return;
		}

		std::vector<EditorObjectInfo> objects;
		scene->CollectEditorObjects(objects);
		const EditorObjectInfo& selected = selection.GetSelected();
		bool hasSelectedDrawable = false;
		for (const EditorObjectInfo& object : objects)
		{
			hasSelectedDrawable |= object.canDrawObjectId && IsSelectedDrawable(object, selected, objects);
		}
		if (!hasSelectedDrawable)
		{
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->RSSetViewports(1, &viewport_);
		commandList->RSSetScissorRects(1, &scissorRect_);

		// 非選択ObjectもID=2で描いて深度を埋め、壁越しに選択輪郭が透けることを防ぐ。
		dxCommon_->ResourceTransition(selectionMask_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->OMSetRenderTargets(1, &maskRtvHandle_, false, &depthDsvHandle_);
		const float clearMask[4] = {};
		commandList->ClearRenderTargetView(maskRtvHandle_, clearMask, 0, nullptr);
		commandList->ClearDepthStencilView(depthDsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		for (const EditorObjectInfo& object : objects)
		{
			if (!object.canDrawObjectId)
			{
				continue;
			}
			object.DrawObjectId(IsSelectedDrawable(object, selected, objects) ? 1u : 2u);
		}
		dxCommon_->ResourceTransition(selectionMask_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// Selection Maskの境界だけを透明Textureへ書き、Main ViewportのImGui Imageへ重ねる。
		dxCommon_->ResourceTransition(outlineTexture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->OMSetRenderTargets(1, &outlineRtvHandle_, false, nullptr);
		const float clearOutline[4] = {};
		commandList->ClearRenderTargetView(outlineRtvHandle_, clearOutline, 0, nullptr);
		SRVManager::GetInstance()->PreDraw();
		commandList->SetGraphicsRootSignature(outlinePipeline_.rootSignature.Get());
		commandList->SetPipelineState(outlinePipeline_.pipelineState.Get());
		commandList->SetGraphicsRootDescriptorTable(0, SRVManager::GetInstance()->GetGPUDescriptorHandle(maskSrvIndex_));
		const float outlineColor[4] = { 1.0f, 0.55f, 0.08f, 1.0f };
		commandList->SetGraphicsRoot32BitConstants(1, 4, outlineColor, 0);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
		dxCommon_->ResourceTransition(outlineTexture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		hasVisibleOutline_ = true;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE EditorSelectionOutlineManager::GetOutlineSrvHandleGPU() const
	{
		return outlineSrvIndex_ == UINT32_MAX
			? D3D12_GPU_DESCRIPTOR_HANDLE{}
			: SRVManager::GetInstance()->GetGPUDescriptorHandle(outlineSrvIndex_);
	}

	void EditorSelectionOutlineManager::CreateMaskResources()
	{
		const float clear[4] = {};
		selectionMask_ = CreateRenderTexture(
			dxCommon_->GetDevice(), DXGI_FORMAT_R32_UINT, clear, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (!selectionMask_)
		{
			return;
		}
		selectionMask_->SetName(L"EditorSelectionMask_R32_UINT");

		maskRtvIndex_ = RTVManager::GetInstance()->Allocate();
		maskRtvHandle_ = RTVManager::GetInstance()->GetCPUDescriptorHandle(maskRtvIndex_);
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R32_UINT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		dxCommon_->GetDevice()->CreateRenderTargetView(selectionMask_.Get(), &rtvDesc, maskRtvHandle_);

		maskSrvIndex_ = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(maskSrvIndex_, selectionMask_.Get(), DXGI_FORMAT_R32_UINT, 1);

		D3D12_CLEAR_VALUE depthClear{};
		selectionDepth_ = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			GameViewportConstants::Width,
			GameViewportConstants::Height,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			depthClear);
		if (selectionDepth_)
		{
			selectionDepth_->SetName(L"EditorSelectionDepth");
			depthDsvIndex_ = DSVManager::GetInstance()->Allocate();
			DSVManager::GetInstance()->CreateDSVForDepthBuffer(depthDsvIndex_, selectionDepth_.Get());
			depthDsvHandle_ = DSVManager::GetInstance()->GetCPUDescriptorHandle(depthDsvIndex_);
		}
	}

	void EditorSelectionOutlineManager::CreateOutlineResources()
	{
		const float clear[4] = {};
		outlineTexture_ = CreateRenderTexture(
			dxCommon_->GetDevice(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clear, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (!outlineTexture_)
		{
			return;
		}
		outlineTexture_->SetName(L"EditorSelectionOutline_RGBA8");
		outlineRtvIndex_ = RTVManager::GetInstance()->Allocate();
		RTVManager::GetInstance()->CreateRTVForTexture2D(outlineRtvIndex_, outlineTexture_.Get());
		outlineRtvHandle_ = RTVManager::GetInstance()->GetCPUDescriptorHandle(outlineRtvIndex_);
		outlineSrvIndex_ = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(
			outlineSrvIndex_, outlineTexture_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
	}

	void EditorSelectionOutlineManager::CreateOutlinePipeline()
	{
		ShaderProgram program{};
		const ShaderDescriptor vertexDesc{
			L"EditorSelectionOutlineVS", L"Resources/Shaders/PostEffect/FullScreen.VS.hlsl", L"main", L"vs_6_0", ShaderStage::Vertex, RootSignatureType::Unknown };
		const ShaderDescriptor pixelDesc{
			L"EditorSelectionOutlinePS", L"Resources/Shaders/EditorPicking/SelectionOutline.PS.hlsl", L"main", L"ps_6_0", ShaderStage::Pixel, RootSignatureType::Unknown };
		program.vertexShader.blob = ShaderCompiler::CompileShader(vertexDesc, dxCommon_->GetDXCCompilerManager());
		program.pixelShader.blob = ShaderCompiler::CompileShader(pixelDesc, dxCommon_->GetDXCCompilerManager());

		D3D12_DESCRIPTOR_RANGE maskRange{};
		maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		maskRange.NumDescriptors = 1;
		maskRange.BaseShaderRegister = 0;
		maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		std::array<D3D12_ROOT_PARAMETER, 2> rootParameters{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[0].DescriptorTable.pDescriptorRanges = &maskRange;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Constants.ShaderRegister = 0;
		rootParameters[1].Constants.Num32BitValues = 4;

		D3D12_ROOT_SIGNATURE_DESC rootDesc{};
		rootDesc.NumParameters = static_cast<UINT>(rootParameters.size());
		rootDesc.pParameters = rootParameters.data();

		GraphicsPipelineDesc desc{};
		desc.debugName = L"EditorSelectionOutlinePipeline";
		desc.shaders = std::move(program);
		desc.inputLayout = { nullptr, 0 };
		desc.blendState = PipelineStatePresets::MakeBlendOpaque();
		desc.rasterizerState = PipelineStatePresets::MakeRasterizerCullNone();
		desc.depthStencilState = PipelineStatePresets::MakeDepthDisable();
		desc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.numRenderTargets = 1;
		desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		outlinePipeline_ = dxCommon_->GetPipelineFactory().CreateGraphicsPipeline(desc, rootDesc);
	}

	bool EditorSelectionOutlineManager::IsSelectedDrawable(
		const EditorObjectInfo& object,
		const EditorObjectInfo& selected,
		const std::vector<EditorObjectInfo>& objects) const
	{
		if (object.id == selected.id)
		{
			return true;
		}
		if (selected.objectKind != EditorObjectKind::Actor)
		{
			return false; // CtrlでComponentを選んだ場合は、そのComponentだけを輪郭対象にする。
		}

		uint64_t parentId = object.parentId;
		for (std::size_t depth = 0; parentId != 0 && depth <= objects.size(); ++depth)
		{
			if (parentId == selected.id)
			{
				return true;
			}
			const auto parent = std::find_if(objects.begin(), objects.end(), [parentId](const EditorObjectInfo& candidate)
				{
					return candidate.id == parentId;
				});
			if (parent == objects.end())
			{
				break;
			}
			parentId = parent->parentId;
		}
		return false;
	}
} // namespace Ken4lowEngine
#endif // USE_IMGUI
