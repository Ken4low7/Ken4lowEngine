#define NOMINMAX
#include "StageChunkManager.h"

#include "Object3DCommon.h"
#include "OcclusionCullingSystem.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kMinChunkSize = 1.0f;
		constexpr float kBoundsPaddingY = 1.0f;

		float ClampChunkSize(float chunkSize)
		{
			return std::max(chunkSize, kMinChunkSize);
		}

		BoundingAABB MakeAABBFromSphere(const BoundingSphere& sphere)
		{
			const Vector3 radius{ sphere.radius, sphere.radius, sphere.radius };
			return { sphere.center - radius, sphere.center + radius };
		}

		Vector3 GetAABBSize(const BoundingAABB& bounds)
		{
			return bounds.max - bounds.min;
		}

		AABB ToDebugAABB(const BoundingAABB& bounds)
		{
			AABB aabb{};
			aabb.min = bounds.min;
			aabb.max = bounds.max;
			return aabb;
		}
	}

	void StageChunkManager::Clear()
	{
		chunks_.clear();
		chunkCullingExcludedMeshes_.clear();
		meshChunkAssignments_.clear();
		statistics_ = {};
		needsRebuild_ = false;
	}

	void StageChunkManager::Rebuild(Object3D* stageObject, float chunkSize)
	{
		chunks_.clear();
		chunkCullingExcludedMeshes_.clear();
		meshChunkAssignments_.clear();
		chunkSize_ = ClampChunkSize(chunkSize);
		needsRebuild_ = false;

		if (!stageObject || stageObject->GetSubmeshCount() == 0)
		{
			BuildStatistics();
			return;
		}

		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		struct PendingMesh
		{
			size_t meshIndex = 0;
			BoundingAABB bounds{};
			int minXIndex = 0;
			int maxXIndex = 0;
			int minZIndex = 0;
			int maxZIndex = 0;
		};
		std::vector<PendingMesh> pendingMeshes;
		pendingMeshes.reserve(stageObject->GetSubmeshCount());
		meshChunkAssignments_.reserve(stageObject->GetSubmeshCount());

		for (size_t meshIndex = 0; meshIndex < stageObject->GetSubmeshCount(); ++meshIndex)
		{
			if (stageObject->IsIgnoreStageChunkCulling())
			{
				chunkCullingExcludedMeshes_.push_back({ stageObject, meshIndex });
				++statistics_.chunkCullingIgnoredObjectCount;
				continue;
			}

			if (!stageObject->HasMeshWorldBoundsForCulling(meshIndex))
			{
				chunkCullingExcludedMeshes_.push_back({ stageObject, meshIndex });
				++statistics_.boundsUnsetDrawObjectCount;
				continue;
			}

			const BoundingAABB meshAABB = MakeAABBFromSphere(stageObject->GetMeshWorldBoundsForCulling(meshIndex));
			minY = std::min(minY, meshAABB.min.y);
			maxY = std::max(maxY, meshAABB.max.y);

			const Vector3 meshSize = GetAABBSize(meshAABB);
			if (autoExcludeLargeObjects_ && (meshSize.x > chunkSize_ || meshSize.z > chunkSize_))
			{
				chunkCullingExcludedMeshes_.push_back({ stageObject, meshIndex });
				++statistics_.chunkCullingIgnoredObjectCount;
				++statistics_.largeObjectExcludedCount;
				continue;
			}

			const int minXIndex = static_cast<int>(std::floor(meshAABB.min.x / chunkSize_));
			const int maxXIndex = static_cast<int>(std::floor(meshAABB.max.x / chunkSize_));
			const int minZIndex = static_cast<int>(std::floor(meshAABB.min.z / chunkSize_));
			const int maxZIndex = static_cast<int>(std::floor(meshAABB.max.z / chunkSize_));
			pendingMeshes.push_back({ meshIndex, meshAABB, minXIndex, maxXIndex, minZIndex, maxZIndex });

		}

		if (pendingMeshes.empty())
		{
			BuildStatistics();
			return;
		}

		minY -= kBoundsPaddingY;
		maxY += kBoundsPaddingY;
		const float height = std::max(maxY - minY, kMinChunkSize);
		const float centerY = (minY + maxY) * 0.5f;

		std::unordered_map<unsigned long long, size_t> keyToChunkIndex;
		for (const PendingMesh& pending : pendingMeshes)
		{
			int assignedChunkCount = 0;
			for (int xIndex = pending.minXIndex; xIndex <= pending.maxXIndex; ++xIndex)
			{
				for (int zIndex = pending.minZIndex; zIndex <= pending.maxZIndex; ++zIndex)
				{
					const unsigned long long key = CalculateChunkKey(xIndex, zIndex);
					auto it = keyToChunkIndex.find(key);
					if (it == keyToChunkIndex.end())
					{
						const int chunkId = static_cast<int>(chunks_.size());
						const Vector3 center = {
							(static_cast<float>(xIndex) + 0.5f) * chunkSize_,
							centerY,
							(static_cast<float>(zIndex) + 0.5f) * chunkSize_
						};
						const Vector3 size = { chunkSize_, height, chunkSize_ };
						it = keyToChunkIndex.emplace(key, chunks_.size()).first;
						chunks_.emplace_back(chunkId, center, size);
					}

					// Mesh の AABB が重なる全 Chunk に登録し、どれかが可視なら Draw する安全側の判定にする。
					chunks_[it->second].AddMesh(stageObject, pending.meshIndex);
					++assignedChunkCount;
				}
			}

			if (assignedChunkCount <= 0)
			{
				chunkCullingExcludedMeshes_.push_back({ stageObject, pending.meshIndex });
				++statistics_.chunkOutsideDrawObjectCount;
			}
			meshChunkAssignments_.push_back({ pending.meshIndex, assignedChunkCount });
		}

		BuildStatistics();
	}

	void StageChunkManager::UpdateVisibility(bool enabled)
	{
		statistics_.drawnChunkCount = 0;
		statistics_.culledChunkCount = 0;

		auto& cullingSystem = Object3DCommon::GetInstance()->GetFrustumCullingSystem();
		for (StageChunk& chunk : chunks_)
		{
			chunk.SetOccludedByOcclusion(false);
			const bool visible = cullingSystem.IsVisible(
				chunk.GetBounds(),
				!enabled,
				true,
				FrustumCullingSystem::CullingUnit::StageChunk);
			chunk.SetVisible(visible);
			if (visible)
			{
				++statistics_.drawnChunkCount;
			}
			else
			{
				++statistics_.culledChunkCount;
			}
		}
	}

	void StageChunkManager::ApplyOcclusionCulling(OcclusionCullingSystem& occlusionCullingSystem, const Matrix4x4& viewProjection)
	{
		const int beforeDrawnCount = statistics_.drawnChunkCount;
		occlusionCullingSystem.ApplyToStageChunks(chunks_, viewProjection);
		const int occludedChunkCount = occlusionCullingSystem.GetStatistics().occludedChunkCount;
		statistics_.drawnChunkCount = std::max(0, beforeDrawnCount - occludedChunkCount);
		statistics_.culledChunkCount += occludedChunkCount;
	}

	void StageChunkManager::DrawVisibleChunks() const
	{
		std::map<Object3D*, std::vector<size_t>> visibleMeshesByObject;

		for (const StageChunkMeshRef& ref : chunkCullingExcludedMeshes_)
		{
			if (ref.object)
			{
				visibleMeshesByObject[ref.object].push_back(ref.meshIndex);
			}
		}

		for (const StageChunk& chunk : chunks_)
		{
			if (!chunk.IsVisible()) { continue; }

			for (const StageChunkMeshRef& ref : chunk.GetMeshes())
			{
				if (!ref.object) { continue; }

				auto& indices = visibleMeshesByObject[ref.object];
				if (std::find(indices.begin(), indices.end(), ref.meshIndex) == indices.end())
				{
					indices.push_back(ref.meshIndex);
				}
			}
		}

		for (const auto& [object, meshIndices] : visibleMeshesByObject)
		{
			if (object && !meshIndices.empty())
			{
				object->DrawMeshes(meshIndices);
			}
		}
	}

	void StageChunkManager::DrawDebugBounds() const
	{
		for (const StageChunk& chunk : chunks_)
		{
			chunk.DrawBoundsDebug(showBounds_, showOccludedBounds_);
		}

		if (!showObjectBounds_) { return; }

		const Vector4 registeredColor{ 0.2f, 0.6f, 1.0f, 1.0f };
		const Vector4 excludedColor{ 1.0f, 0.85f, 0.1f, 1.0f };
		for (const StageChunk& chunk : chunks_)
		{
			for (const StageChunkMeshRef& ref : chunk.GetMeshes())
			{
				if (!ref.object || !ref.object->HasMeshWorldBoundsForCulling(ref.meshIndex)) { continue; }
				Wireframe::GetInstance()->DrawAABB(ToDebugAABB(MakeAABBFromSphere(ref.object->GetMeshWorldBoundsForCulling(ref.meshIndex))), registeredColor);
			}
		}
		for (const StageChunkMeshRef& ref : chunkCullingExcludedMeshes_)
		{
			if (!ref.object || !ref.object->HasMeshWorldBoundsForCulling(ref.meshIndex)) { continue; }
			Wireframe::GetInstance()->DrawAABB(ToDebugAABB(MakeAABBFromSphere(ref.object->GetMeshWorldBoundsForCulling(ref.meshIndex))), excludedColor);
		}
	}

	void StageChunkManager::BuildStatistics()
	{
		const int boundsUnsetDrawObjectCount = statistics_.boundsUnsetDrawObjectCount;
		const int chunkOutsideDrawObjectCount = statistics_.chunkOutsideDrawObjectCount;
		const int chunkCullingIgnoredObjectCount = statistics_.chunkCullingIgnoredObjectCount;
		const int largeObjectExcludedCount = statistics_.largeObjectExcludedCount;

		statistics_ = {};
		statistics_.boundsUnsetDrawObjectCount = boundsUnsetDrawObjectCount;
		statistics_.chunkOutsideDrawObjectCount = chunkOutsideDrawObjectCount;
		statistics_.chunkCullingIgnoredObjectCount = chunkCullingIgnoredObjectCount;
		statistics_.largeObjectExcludedCount = largeObjectExcludedCount;
		statistics_.totalChunkCount = static_cast<int>(chunks_.size());
		for (const StageChunk& chunk : chunks_)
		{
			statistics_.totalObjectCountInChunks += chunk.GetObjectCount();
			statistics_.totalMeshCountInChunks += static_cast<int>(chunk.GetMeshes().size());
		}

		for (const MeshChunkAssignment& assignment : meshChunkAssignments_)
		{
			if (assignment.meshIndex == debugSelectedMeshIndex_)
			{
				statistics_.selectedObjectChunkCount = assignment.chunkCount;
				break;
			}
		}
	}

	unsigned long long StageChunkManager::CalculateChunkKey(int xIndex, int zIndex) const
	{
		return (static_cast<unsigned long long>(static_cast<unsigned int>(xIndex)) << 32) | static_cast<unsigned int>(zIndex);
	}
}
