#pragma once
#include "AABB.h"
#include "BoundingVolume.h"
#include "Object3D.h"
#include "Vector3.h"

#include <cstddef>
#include <vector>

namespace Ken4lowEngine
{
	struct StageChunkMeshRef
	{
		Object3D* object = nullptr;
		size_t meshIndex = 0;
	};

	/// <summary>
	/// 静的ステージをグリッド単位でまとめ、Chunk Bounds の可視状態で Draw を制御する単位。
	/// </summary>
	class StageChunk
	{
	public:
		StageChunk() = default;
		StageChunk(int chunkId, const Vector3& center, const Vector3& size);

		void AddMesh(Object3D* object, size_t meshIndex);
		void ClearMeshes();
		void Draw() const;
		void DrawBoundsDebug(bool showBounds, bool showOccludedBounds) const;

		int GetChunkId() const { return chunkId_; }
		const Vector3& GetCenter() const { return center_; }
		const Vector3& GetSize() const { return size_; }
		const BoundingAABB& GetBounds() const { return bounds_; }
		AABB GetDebugAABB() const;
		const std::vector<StageChunkMeshRef>& GetMeshes() const { return meshes_; }
		int GetObjectCount() const { return objectCount_; }
		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }
		bool IsOccludedByOcclusion() const { return occludedByOcclusion_; }
		void SetOccludedByOcclusion(bool occluded) { occludedByOcclusion_ = occluded; }

	private:
		int chunkId_ = 0;
		Vector3 center_{};
		Vector3 size_{ 1.0f, 1.0f, 1.0f };
		BoundingAABB bounds_{};
		std::vector<StageChunkMeshRef> meshes_{};
		int objectCount_ = 0;
		bool visible_ = true;
		bool occludedByOcclusion_ = false;
	};
}
