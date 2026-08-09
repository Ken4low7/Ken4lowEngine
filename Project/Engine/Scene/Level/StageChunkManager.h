#pragma once
#include "StageChunk.h"
#include "Matrix4x4.h"

#include <cstddef>
#include <vector>

namespace Ken4lowEngine
{
	class OcclusionCullingSystem;
	class StageChunkManager
	{
	public:
		struct Statistics
		{
			int totalChunkCount = 0;
			int drawnChunkCount = 0;
			int culledChunkCount = 0;
			int totalObjectCountInChunks = 0;
			int totalMeshCountInChunks = 0;
			int boundsUnsetDrawObjectCount = 0;
			int chunkOutsideDrawObjectCount = 0;
			int chunkCullingIgnoredObjectCount = 0;
			int largeObjectExcludedCount = 0;
			int selectedObjectChunkCount = 0;
		};

		void Clear();
		void Rebuild(Object3D* stageObject, float chunkSize);
		void UpdateVisibility(bool enabled);
		void ApplyOcclusionCulling(OcclusionCullingSystem& occlusionCullingSystem, const Matrix4x4& viewProjection);
		void DrawVisibleChunks() const;
		void DrawDebugBounds() const;

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetShowBounds(bool showBounds) { showBounds_ = showBounds; }
		bool IsShowBounds() const { return showBounds_; }
		void SetShowObjectBounds(bool showObjectBounds) { showObjectBounds_ = showObjectBounds; }
		bool IsShowObjectBounds() const { return showObjectBounds_; }
		void SetShowOccludedBounds(bool showOccludedBounds) { showOccludedBounds_ = showOccludedBounds; }
		bool IsShowOccludedBounds() const { return showOccludedBounds_; }
		void SetAutoExcludeLargeObjects(bool enabled) { autoExcludeLargeObjects_ = enabled; MarkRebuildRequested(); }
		bool IsAutoExcludeLargeObjects() const { return autoExcludeLargeObjects_; }
		void SetChunkSize(float chunkSize) { chunkSize_ = chunkSize; }
		float GetChunkSize() const { return chunkSize_; }
		bool NeedsRebuild() const { return needsRebuild_; }
		void MarkRebuildRequested() { needsRebuild_ = true; }
		void SetDebugSelectedMeshIndex(size_t meshIndex) { debugSelectedMeshIndex_ = meshIndex; }
		size_t GetDebugSelectedMeshIndex() const { return debugSelectedMeshIndex_; }

		const Statistics& GetStatistics() const { return statistics_; }
		const std::vector<StageChunk>& GetChunks() const { return chunks_; }

	private:
		struct MeshChunkAssignment
		{
			size_t meshIndex = 0;
			int chunkCount = 0;
		};

		void BuildStatistics();
		unsigned long long CalculateChunkKey(int xIndex, int zIndex) const;

		std::vector<StageChunk> chunks_{};
		std::vector<StageChunkMeshRef> chunkCullingExcludedMeshes_{};
		std::vector<MeshChunkAssignment> meshChunkAssignments_{};
		Statistics statistics_{};
		float chunkSize_ = 20.0f;
		bool enabled_ = true;
		bool showBounds_ = false;
		bool showObjectBounds_ = false;
		bool showOccludedBounds_ = false;
		bool autoExcludeLargeObjects_ = true;
		bool needsRebuild_ = false;
		size_t debugSelectedMeshIndex_ = 0;
	};
}
