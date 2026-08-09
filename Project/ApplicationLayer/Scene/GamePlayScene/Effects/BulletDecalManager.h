#pragma once

#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <random>
#include <string>

namespace Ken4lowEngine
{
	class DirectXCommon;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// 壁・床への簡易銃痕をApplicationLayer内で管理するクラス。
/// 共有Quadとインスタンスバッファを使い、Object3Dを銃痕ごとに生成しない。
/// -------------------------------------------------------------
class BulletDecalManager
{
public:
	// 銃痕1件の配置・向き・寿命を保持する。
	struct BulletDecal
	{
		K4E::Vector3 position{};
		K4E::Vector3 normal{ 0.0f, 0.0f, 1.0f };
		K4E::Vector3 scale{ 0.18f, 0.18f, 1.0f };
		float rotation = 0.0f;
		float lifeTime = 30.0f;
		K4E::Matrix4x4 world{};
		K4E::Vector4 color{ 0.045f, 0.04f, 0.035f, 0.92f };
	};

	static constexpr size_t kMaxBulletDecals = 128;

	BulletDecalManager() = default;
	~BulletDecalManager();
	BulletDecalManager(const BulletDecalManager&) = delete;
	BulletDecalManager& operator=(const BulletDecalManager&) = delete;

	// 共有Quad、インスタンスバッファ、描画PSOを初期化する。
	bool Initialize();
	void Finalize();

	// 着弾位置と面法線から銃痕を追加する。
	void AddDecal(const K4E::Vector3& hitPosition, const K4E::Vector3& hitNormal);
	void Update(float deltaTime);

	// 銃痕用の板ポリを壁面方向に向け、共有Quadで一括描画する。
	void Draw();
	void DrawImGui();
	void Clear();

	size_t GetCount() const { return decals_.size(); }
	static constexpr size_t GetMaxCount() { return kMaxBulletDecals; }
	bool IsEnabled() const { return enabled_; }
	void SetEnabled(bool enabled) { enabled_ = enabled; }

private:
	struct VertexData
	{
		K4E::Vector3 position;
		float uv[2];
	};

	struct InstanceData
	{
		K4E::Matrix4x4 world;
		K4E::Vector4 color;
	};

	struct ViewProjectionData
	{
		K4E::Matrix4x4 viewProjection;
	};

	// Quadのローカル+Zを着弾法線へ向け、面内ランダム回転も適用する。
	K4E::Matrix4x4 MakeDecalWorldMatrix(const BulletDecal& decal) const;
	bool CreatePipeline();
	bool CreateBuffers();
	std::string ResolveTexturePath() const;

private:
	K4E::DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionBuffer_;
	VertexData* mappedVertices_ = nullptr;
	uint32_t* mappedIndices_ = nullptr;
	InstanceData* mappedInstances_ = nullptr;
	ViewProjectionData* mappedViewProjection_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	D3D12_VERTEX_BUFFER_VIEW instanceBufferView_{};
	std::deque<BulletDecal> decals_{};
	std::mt19937 random_{ 0xB01137u };
	std::string texturePath_{};
	bool enabled_ = true;
	bool initialized_ = false;
};

