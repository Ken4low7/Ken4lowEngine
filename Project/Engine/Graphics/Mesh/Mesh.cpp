#include "Mesh.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"
#include <span>

namespace Ken4lowEngine
{
	void Mesh::Initialize(const std::vector<VertexData>& modelVertices, const std::vector<uint32_t>& modelIndices)
	{
		vertices = modelVertices;
		indices = modelIndices;
		auto* device = DirectXCommon::GetInstance()->GetDevice();

		vertexResource = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * vertices.size());
		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
		vertexBufferView.StrideInBytes = sizeof(VertexData);
		VertexData* vertexDataRaw = nullptr;
		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataRaw));
		vertexResource->SetName(L"Mesh::VertexBuffer");
		std::span<VertexData> vertexSpan{ vertexDataRaw, vertices.size() };
		std::copy(vertices.begin(), vertices.end(), vertexSpan.begin());

		indexResource = ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * indices.size());
		indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = UINT(sizeof(uint32_t) * indices.size());
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		uint32_t* indexDataRaw = nullptr;
		indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexDataRaw));
		indexResource->SetName(L"Mesh::IndexBuffer");
		std::span<uint32_t> indexSpan{ indexDataRaw, indices.size() };
		std::copy(indices.begin(), indices.end(), indexSpan.begin());
	}

	void Mesh::Draw()
	{
		DrawInstanced(1);
	}

	void Mesh::DrawInstanced(UINT instanceCount, UINT startInstanceLocation)
	{
		if (instanceCount == 0) return;
		ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), instanceCount, 0, 0, startInstanceLocation); // 選択Instanceだけを描けるよう開始Instanceを渡す。
	}
} // namespace Ken4lowEngine
