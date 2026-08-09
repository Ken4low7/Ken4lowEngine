#pragma once
// Wireframe内部で使う頂点/バッファ関連の型定義。

#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstdint>

namespace Ken4lowEngine
{
	// ワイヤー描画の頂点データ。
	struct WireframeVertexData
	{
		Vector3 position;
		Vector4 color;
	};

	// AABB / OBB共有線メッシュの頂点データ。色と変換はインスタンス側から受け取る。
	struct WireframeBoxVertexData
	{
		Vector3 position;
	};

	// AABB / OBB 1個分のインスタンスデータ。単位キューブをworld行列で各Boxへ変換する。
	struct WireframeBoxInstanceData
	{
		Matrix4x4 world;
		Vector4 color;
	};

	// AABB / OBB共通の共有メッシュと、毎フレーム更新するインスタンスバッファを管理する。
	struct WireframeBoxInstancedData
	{
		WireframeBoxVertexData* baseVertexData = nullptr;
		uint32_t* indexData = nullptr;
		WireframeBoxInstanceData* instanceData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> baseVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer;
		D3D12_VERTEX_BUFFER_VIEW baseVertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		D3D12_VERTEX_BUFFER_VIEW instanceBufferView{};
	};

	// Sphere共有リングメッシュの頂点データ。半径・位置・色はインスタンス側から受け取る。
	struct WireframeSphereVertexData
	{
		Vector3 position;
	};

	// Sphere 1個分のインスタンスデータ。単位球リングをworld行列で配置する。
	struct WireframeSphereInstanceData
	{
		Matrix4x4 world;
		Vector4 color;
	};

	// Sphereの共有3リングメッシュと、毎フレーム更新するインスタンスバッファを管理する。
	struct WireframeSphereInstancedData
	{
		WireframeSphereVertexData* baseVertexData = nullptr;
		uint32_t* indexData = nullptr;
		WireframeSphereInstanceData* instanceData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> baseVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer;
		D3D12_VERTEX_BUFFER_VIEW baseVertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		D3D12_VERTEX_BUFFER_VIEW instanceBufferView{};
	};

	// Capsule共有ワイヤーメッシュの頂点データ。形状配置と色はインスタンス側から受け取る。
	struct WireframeCapsuleVertexData
	{
		Vector3 position;
	};

	// Capsule 1個分のインスタンスデータ。Y軸基準の単位Capsuleをworld行列で配置する。
	struct WireframeCapsuleInstanceData
	{
		Matrix4x4 world;
		Vector4 color;
	};

	// Capsule共有メッシュと、毎フレーム更新するインスタンスバッファを管理する。
	struct WireframeCapsuleInstancedData
	{
		WireframeCapsuleVertexData* baseVertexData = nullptr;
		uint32_t* indexData = nullptr;
		WireframeCapsuleInstanceData* instanceData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> baseVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer;
		D3D12_VERTEX_BUFFER_VIEW baseVertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		D3D12_VERTEX_BUFFER_VIEW instanceBufferView{};
	};

	// ワイヤー描画用の座標変換行列。
	struct WireframeTransformationMatrix
	{
		Matrix4x4 WVP;
	};

	// 三角形描画用の頂点バッファ情報。
	struct WireframeTriangleData
	{
		WireframeVertexData* vertexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

	// 矩形描画用の頂点/インデックスバッファ情報。
	struct WireframeBoxData
	{
		WireframeVertexData* vertexData = nullptr;
		uint32_t* indexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	};

	// 線分描画用の頂点バッファ情報。
	struct WireframeLineData
	{
		WireframeVertexData* vertexData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};

}
