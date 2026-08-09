#pragma once
#include "BoundingVolume.h"
#include "Frustum.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Scene や ImGui に依存しない Frustum Culling の実行状態と統計。
	/// </summary>
	class FrustumCullingSystem
	{
	public:
		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }

		void BuildFrustum(const Matrix4x4& viewProjection);
		void ResetStatistics();

		enum class CullingUnit
		{
			Object,
			Mesh,
			StageObject,
			StageChunk
		};

		bool IsVisible(const BoundingSphere& bounds, bool ignoreFrustumCulling = false, bool hasBounds = true, CullingUnit unit = CullingUnit::Object);
		bool IsVisible(const BoundingAABB& bounds, bool ignoreFrustumCulling = false, bool hasBounds = true, CullingUnit unit = CullingUnit::Object);

		int GetTotalObjectCount() const { return totalObjectCount_; }
		int GetDrawnObjectCount() const { return drawnObjectCount_; }
		int GetCulledObjectCount() const { return culledObjectCount_; }
		int GetCullingDisabledDrawnObjectCount() const { return cullingDisabledDrawnObjectCount_; }
		int GetMissingBoundsDrawnObjectCount() const { return missingBoundsDrawnObjectCount_; }
		int GetTotalMeshCount() const { return totalMeshCount_; }
		int GetDrawnMeshCount() const { return drawnMeshCount_; }
		int GetCulledMeshCount() const { return culledMeshCount_; }
		int GetTotalStageObjectCount() const { return totalStageObjectCount_; }
		int GetDrawnStageObjectCount() const { return drawnStageObjectCount_; }
		int GetCulledStageObjectCount() const { return culledStageObjectCount_; }
		int GetTotalStageChunkCount() const { return totalStageChunkCount_; }
		int GetDrawnStageChunkCount() const { return drawnStageChunkCount_; }
		int GetCulledStageChunkCount() const { return culledStageChunkCount_; }

		const Matrix4x4& GetViewProjectionMatrix() const { return viewProjection_; }
		const Frustum& GetFrustum() const { return frustum_; }

	private:
		bool IsVisibleInternal(bool intersects, bool ignoreFrustumCulling, bool hasBounds, CullingUnit unit);
		void CountTotal(CullingUnit unit);
		void CountDrawn(CullingUnit unit);
		void CountCulled(CullingUnit unit);

		Frustum frustum_{};
		Matrix4x4 viewProjection_{};
		bool enabled_ = true;
		int totalObjectCount_ = 0;
		int drawnObjectCount_ = 0;
		int culledObjectCount_ = 0;
		int cullingDisabledDrawnObjectCount_ = 0;
		int missingBoundsDrawnObjectCount_ = 0;
		int totalMeshCount_ = 0;
		int drawnMeshCount_ = 0;
		int culledMeshCount_ = 0;
		int totalStageObjectCount_ = 0;
		int drawnStageObjectCount_ = 0;
		int culledStageObjectCount_ = 0;
		int totalStageChunkCount_ = 0;
		int drawnStageChunkCount_ = 0;
		int culledStageChunkCount_ = 0;
	};
}
