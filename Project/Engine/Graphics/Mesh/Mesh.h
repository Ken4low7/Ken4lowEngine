#pragma once
#include "DX12Include.h"
#include "VertexData.h"

#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　メッシュデータクラス
	/// -------------------------------------------------------------
	class Mesh
	{
	public:
		void Initialize(const std::vector<VertexData>& modelVertices, const std::vector<uint32_t>& modelIndices);
		void Draw();
		void DrawInstanced(UINT instanceCount, UINT startInstanceLocation = 0);

		uint32_t GetIndexCount() const { return static_cast<uint32_t>(indices.size()); }
		uint32_t GetVertexCount() const { return static_cast<uint32_t>(vertices.size()); }

	private:
		ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		std::vector<VertexData> vertices = {};
		ComPtr<ID3D12Resource> indexResource = nullptr;
		D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
		std::vector<uint32_t> indices = {};
	};
} // namespace Ken4lowEngine
