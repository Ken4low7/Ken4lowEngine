#pragma once
#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			　	メッシュパーティクルアセット構造体
/// -------------------------------------------------------------
struct MeshParticleAsset
{
	Microsoft::WRL::ComPtr<ID3D12Resource> vb;
	Microsoft::WRL::ComPtr<ID3D12Resource> ib;

	D3D12_VERTEX_BUFFER_VIEW vbv{};
	D3D12_INDEX_BUFFER_VIEW  ibv{};
	uint32_t indexCount = 0;

	std::string textureFilePath;
};
} // namespace Ken4lowEngine
