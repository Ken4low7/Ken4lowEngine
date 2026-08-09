#include "ParticleMesh.h"
#include <DirectXCommon.h>
#include <ResourceManager.h>
#include <numbers>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　　　		初期化処理
/// -------------------------------------------------------------
void ParticleMesh::Initialize()
{
	// 6つの頂点を定義して四角形を表現
	vertices.push_back({ .position = { -1.0f,  1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左上
	vertices.push_back({ .position = {  1.0f,  1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	vertices.push_back({ .position = { -1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下

	vertices.push_back({ .position = { -1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {0.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 左下
	vertices.push_back({ .position = {  1.0f,  1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 0.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右上
	vertices.push_back({ .position = {  1.0f, -1.0f, 0.0f, 1.0f}, .texcoord = {1.0f, 1.0f}, .normal = {0.0f, 0.0f, 1.0f} }); // 右下

	// 頂点バッファを生成
	CreateVertexBuffer();
}

/// -------------------------------------------------------------
///				　リングの頂点データを生成
/// -------------------------------------------------------------
void ParticleMesh::InitializeRing()
{
	const uint32_t kSubdivision = 32; // 分割数
	const float innerRadius = 1.0f;	  // 内側の半径
	const float outerRadius = 0.4f;	  // 外側の半径

	// 頂点データのクリア
	for (uint32_t i = 0; i < kSubdivision; ++i)
	{
		// 角度を計算
		float theta1 = 2.0f * std::numbers::pi_v<float> *i / kSubdivision;
		float theta2 = 2.0f * std::numbers::pi_v<float> *(i + 1) / kSubdivision;

		// UV座標を計算
		float u1 = static_cast<float>(i) / kSubdivision;
		float u2 = static_cast<float>(i + 1) / kSubdivision;

		// 頂点の位置を計算
		Vector4 p0 = { std::cos(theta1) * innerRadius, std::sin(theta1) * innerRadius, 0.0f, 1.0f };
		Vector4 p1 = { std::cos(theta2) * innerRadius, std::sin(theta2) * innerRadius, 0.0f, 1.0f };
		Vector4 p2 = { std::cos(theta1) * outerRadius, std::sin(theta1) * outerRadius, 0.0f, 1.0f };
		Vector4 p3 = { std::cos(theta2) * outerRadius, std::sin(theta2) * outerRadius, 0.0f, 1.0f };

		// 法線（Z方向の外向き）
		Vector3 normal = { 0.0f, 0.0f, 1.0f };

		// 三角形1（内→外→外）
		vertices.push_back({ p0, {u1, 1.0f}, normal }); // 内側→上
		vertices.push_back({ p2, {u1, 0.0f}, normal }); // 外側→下
		vertices.push_back({ p3, {u2, 0.0f}, normal });

		// 三角形2（内→外→内）						 
		vertices.push_back({ p0, {u1, 1.0f}, normal });
		vertices.push_back({ p3, {u2, 0.0f}, normal });
		vertices.push_back({ p1, {u2, 1.0f}, normal });
	}

	// 頂点バッファを生成
	CreateVertexBuffer();
}

/// -------------------------------------------------------------
///				　シリンダーの頂点データの初期化処理
/// -------------------------------------------------------------
void ParticleMesh::InitializeCylinder()
{
	const uint32_t kCylinderDivide = 32;													 // シリンダーの分割数
	const float kTopRadius = 1.0f;															 // 上面の半径
	const float kBottomRadius = 1.0f;														 // 下面の半径
	const float kHeight = 3.0f;																 // 高さ
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kCylinderDivide); // 分割ごとのラジアン角度

	// 頂点データのクリア
	for (uint32_t index = 0; index < kCylinderDivide; ++index)
	{
		float sin0 = std::sin(index * radianPerDivide);
		float cos0 = std::cos(index * radianPerDivide);
		float sin1 = std::sin((index + 1) * radianPerDivide);
		float cos1 = std::cos((index + 1) * radianPerDivide);

		float u0 = float(index) / kCylinderDivide;
		float u1 = float(index + 1) / kCylinderDivide;

		// 頂点ポジションとUV、法線（X,Z方向の外向き）
		Vector4 top0 = { -sin0 * kTopRadius, kHeight, cos0 * kTopRadius, 1.0f };
		Vector4 top1 = { -sin1 * kTopRadius, kHeight, cos1 * kTopRadius, 1.0f };
		Vector4 bottom0 = { -sin0 * kBottomRadius, 0.0f, cos0 * kBottomRadius, 1.0f };
		Vector4 bottom1 = { -sin1 * kBottomRadius, 0.0f, cos1 * kBottomRadius, 1.0f };

		Vector3 normal0 = { -sin0, 0.0f, cos0 };
		Vector3 normal1 = { -sin1, 0.0f, cos1 };

		// 三角形1（top0 → bottom0 → bottom1）
		vertices.push_back({ top0, {u0, 0.0f}, normal0 });
		vertices.push_back({ bottom0, {u0, 1.0f}, normal0 });
		vertices.push_back({ bottom1, {u1, 1.0f}, normal1 });

		// 三角形2（top0 → bottom1 → top1）
		vertices.push_back({ top0, {u0, 0.0f}, normal0 });
		vertices.push_back({ bottom1, {u1, 1.0f}, normal1 });
		vertices.push_back({ top1, {u1, 0.0f}, normal1 });
	}

	CreateVertexBuffer();
}

/// -------------------------------------------------------------
///				　星型の頂点データの初期化処理
/// -------------------------------------------------------------
void ParticleMesh::InitializeStar()
{
	vertices.clear();
	indices.clear();

	const int numRays = 8;		// 星の光線の数
	const float radius = 1.0f;  // 星の半径

	// 頂点データの生成
	for (int i = 0; i < numRays; ++i)
	{
		// 角度を計算
		float angle = (float)i / numRays * 2.0f * std::numbers::pi_v<float>;

		Vector4 center = { 0.0f, 0.0f, 0.0f, 1.0f };
		Vector4 outer = { std::cos(angle) * radius, std::sin(angle) * radius, 0.0f, 1.0f };
		Vector4 right = { std::cos(angle + 0.1f) * radius * 0.5f, std::sin(angle + 0.1f) * radius * 0.5f, 0.0f, 1.0f };
		Vector3 normal = { 0.0f, 0.0f, 1.0f };

		uint32_t startIndex = static_cast<uint32_t>(vertices.size());

		vertices.push_back({ center, { 0.5f, 0.5f }, normal });
		vertices.push_back({ outer,  { 1.0f, 0.5f }, normal });
		vertices.push_back({ right,  { 0.75f, 1.0f }, normal });

		indices.push_back(startIndex);
		indices.push_back(startIndex + 1);
		indices.push_back(startIndex + 2);
	}

	// 頂点バッファを生成
	CreateVertexBuffer();
}

/// -------------------------------------------------------------
///				　スモークの頂点データの初期化処理
/// -------------------------------------------------------------
void ParticleMesh::InitializeSmoke()
{
	vertices.clear();
	indices.clear();

	const float size = 1.0f;
	Vector3 normal = { 0.0f, 0.0f, 1.0f };

	// 左下三角形
	vertices.push_back({ { -size, -size, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal }); // 0 左下
	vertices.push_back({ { -size,  size, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal }); // 1 左上
	vertices.push_back({ {  size, -size, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }); // 2 右下

	// 右上三角形
	vertices.push_back({ {  size, -size, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }); // 3 右下
	vertices.push_back({ { -size,  size, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal }); // 4 左上
	vertices.push_back({ {  size,  size, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal }); // 5 右上

	// インデックス（頂点順そのまま）
	indices = {
		0, 1, 2,
		3, 4, 5
	};

	// 頂点バッファを生成
	CreateVertexBuffer();
}

/// -------------------------------------------------------------
///				　　　		描画処理
/// -------------------------------------------------------------
void ParticleMesh::Draw(UINT instanceCount)
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
	if (hasIndex_)
	{
		commandList->DrawIndexedInstanced(UINT(indices.size()), instanceCount, 0, 0, 0);
	}
	else
	{
		commandList->DrawInstanced(UINT(vertices.size()), instanceCount, 0, 0);
	}
}

/// -------------------------------------------------------------
///				　　　頂点データの生成
/// -------------------------------------------------------------
void ParticleMesh::CreateVertexBuffer()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();
	vertexResource_ = ResourceManager::CreateBufferResource(device, sizeof(VertexData) * vertices.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, vertices.data(), sizeof(VertexData) * vertices.size());
	vertexResource_->SetName(L"ParticleMesh_VertexBuffer");
	hasIndex_ = !indices.empty();
	if (hasIndex_)
	{
		indexResource_ = ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * indices.size());
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices.size());
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		void* dst = nullptr;
		indexResource_->Map(0, nullptr, &dst);
		std::memcpy(dst, indices.data(), sizeof(uint32_t) * indices.size());
		indexResource_->SetName(L"ParticleMesh_IndexBuffer");
	}
}

} // namespace Ken4lowEngine
