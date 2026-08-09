#pragma once
#include <d3d12.h>

namespace Ken4lowEngine::PipelineStatePresets
{

	D3D12_BLEND_DESC MakeBlendAlpha();
	D3D12_BLEND_DESC MakeBlendOpaque();

	D3D12_RASTERIZER_DESC MakeRasterizerCullBack();
	D3D12_RASTERIZER_DESC MakeRasterizerCullNone();

	D3D12_DEPTH_STENCIL_DESC MakeDepthDisable();
	D3D12_DEPTH_STENCIL_DESC MakeDepthReadWrite();
	D3D12_DEPTH_STENCIL_DESC MakeDepthReadOnly();

}