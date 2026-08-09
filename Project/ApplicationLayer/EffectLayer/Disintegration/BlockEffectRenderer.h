#pragma once

#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// 崩壊と再構築で共用する、立方体インスタンス描画専用のレンダラー。
class BlockEffectRenderer
{
public:
	struct BlockInstance
	{
		K4E::Matrix4x4 world{};
		K4E::Vector4 color{};
	};

	explicit BlockEffectRenderer(std::wstring debugName);
	~BlockEffectRenderer();

	BlockEffectRenderer(const BlockEffectRenderer&) = delete;
	BlockEffectRenderer& operator=(const BlockEffectRenderer&) = delete;

	void Draw(const std::vector<BlockInstance>& instances);

	/// 共通フィールドを持つCPUブロック列から、可視インスタンスだけを描画形式へ変換する。
	template<class Block, class ColorBuilder>
	static void BuildVisibleInstances(
		const std::vector<Block>& blocks,
		float globalAlpha,
		std::vector<BlockInstance>& destination,
		ColorBuilder&& buildColor)
	{
		destination.clear();
		destination.reserve(blocks.size());
		const float alphaScale = std::clamp(globalAlpha, 0.0f, 1.0f);
		for (const auto& block : blocks)
		{
			if (!block.alive || block.alpha * alphaScale <= 0.0f) { continue; }
			const K4E::Vector3 scale = block.scale * block.size;
			destination.push_back({
				K4E::Matrix4x4::MakeAffineMatrix(scale, block.rotation, block.position),
				buildColor(block, alphaScale),
			});
		}
	}

private:
	struct CubeVertex
	{
		K4E::Vector3 position{};
	};

	struct ViewProjectionData
	{
		K4E::Matrix4x4 viewProjection{};
	};

	void Initialize();
	void CreatePipeline();
	void CreateCubeMesh();
	void EnsureInstanceCapacity(size_t requiredCapacity);
	void ReleaseSrv();

	std::wstring debugName_;
	bool initialized_ = false;
	uint32_t srvIndex_ = UINT32_MAX;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionBuffer_;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> retiredInstanceBuffers_;
	std::vector<uint32_t> retiredSrvIndices_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	BlockInstance* instanceData_ = nullptr;
	ViewProjectionData* viewProjectionData_ = nullptr;
	size_t instanceCapacity_ = 0;
	UINT indexCount_ = 0;
};
