#include "PipelineStatePresets.h"

namespace Ken4lowEngine::PipelineStatePresets
{

	D3D12_BLEND_DESC MakeBlendAlpha()
	{
		D3D12_BLEND_DESC desc{};
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		// Main ViewportをImGui::Imageで合成しても透けないようSprite描画ではRTのalphaを維持する。
		desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
		desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	D3D12_BLEND_DESC MakeBlendOpaque()
	{
		D3D12_BLEND_DESC desc{};
		desc.RenderTarget[0].BlendEnable = FALSE;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	D3D12_RASTERIZER_DESC MakeRasterizerCullBack()
	{
		D3D12_RASTERIZER_DESC desc{};
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_BACK;
		desc.DepthClipEnable = TRUE;
		return desc;
	}

	D3D12_RASTERIZER_DESC MakeRasterizerCullNone()
	{
		D3D12_RASTERIZER_DESC desc{};
		desc.FillMode = D3D12_FILL_MODE_SOLID;
		desc.CullMode = D3D12_CULL_MODE_NONE;
		desc.DepthClipEnable = TRUE;
		return desc;
	}

	D3D12_DEPTH_STENCIL_DESC MakeDepthDisable()
	{
		D3D12_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		return desc;
	}

	D3D12_DEPTH_STENCIL_DESC MakeDepthReadWrite()
	{
		D3D12_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		return desc;
	}

	D3D12_DEPTH_STENCIL_DESC MakeDepthReadOnly()
	{
		D3D12_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		return desc;
	}
};