#pragma once

#include "Material.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>
	/// Main 3D pass のSurface/Cull統計をRender path横断で集約します。
	/// </summary>
	class CullingDiagnostics
	{
	public:
		enum class SurfacePath : uint8_t
		{
			Static = 0,
			Alpha,
			Instanced,
			Animated,
			Count
		};

		static constexpr size_t kCullModeCount = 3;
		static constexpr size_t kSurfacePathCount = static_cast<size_t>(SurfacePath::Count);

		struct Snapshot
		{
			uint64_t indexedDrawCalls = 0;
			uint64_t submittedSurfaceInstances = 0;
			uint64_t submittedTriangles = 0;
			uint64_t visibilityMeshletInstances = 0;
			uint64_t normalConeCandidateDrawCalls = 0;
			uint64_t normalConeCandidateMeshletInstances = 0;
			uint64_t normalConeCandidateTriangles = 0;
			std::array<uint64_t, kCullModeCount> pipelineBindsByCullMode{};
			std::array<uint64_t, kCullModeCount> drawCallsByCullMode{};
			std::array<uint64_t, kCullModeCount> trianglesByCullMode{};
			std::array<uint64_t, kSurfacePathCount> pipelineBindsByPath{};
			std::array<uint64_t, kSurfacePathCount> drawCallsByPath{};
			std::array<uint64_t, kSurfacePathCount> trianglesByPath{};
		};

		static CullingDiagnostics* GetInstance()
		{
			static CullingDiagnostics instance;
			return &instance;
		}

		void BeginMainPass()
		{
			snapshot_ = {};
			mainPassActive_ = true;
			activeSurfaceValid_ = false; // 前フレームのPSO状態を新しいMain Passへ持ち越さない。
		}

		void EndMainPass()
		{
			mainPassActive_ = false;
			activeSurfaceValid_ = false; // Selection/PickingなどMain外のDrawを統計へ混ぜない。
		}

		void SetActiveSurface(MaterialCullMode cullMode, SurfacePath path)
		{
			if (!mainPassActive_) { return; }
			activeCullMode_ = cullMode;
			activeSurfacePath_ = path;
			activeSurfaceValid_ = true;
			++snapshot_.pipelineBindsByCullMode[ToCullIndex(cullMode)];
			++snapshot_.pipelineBindsByPath[ToPathIndex(path)];
		}

		void RecordIndexedDraw(
			uint64_t indexCount,
			uint64_t instanceCount,
			uint64_t visibilityMeshletCount = 0,
			uint64_t normalConeCandidateMeshletCount = 0,
			uint64_t normalConeCandidateTriangleCount = 0)
		{
			if (!mainPassActive_ || !activeSurfaceValid_ || instanceCount == 0) { return; }

			const size_t cullIndex = ToCullIndex(activeCullMode_);
			const size_t pathIndex = ToPathIndex(activeSurfacePath_);
			const uint64_t triangleCount = (indexCount / 3ull) * instanceCount;

			++snapshot_.indexedDrawCalls;
			snapshot_.submittedSurfaceInstances += instanceCount;
			snapshot_.submittedTriangles += triangleCount;
			snapshot_.visibilityMeshletInstances += visibilityMeshletCount * instanceCount;
			++snapshot_.drawCallsByCullMode[cullIndex];
			snapshot_.trianglesByCullMode[cullIndex] += triangleCount;
			++snapshot_.drawCallsByPath[pathIndex];
			snapshot_.trianglesByPath[pathIndex] += triangleCount;

			if (activeCullMode_ != MaterialCullMode::None && normalConeCandidateMeshletCount > 0)
			{
				++snapshot_.normalConeCandidateDrawCalls;
				snapshot_.normalConeCandidateMeshletInstances += normalConeCandidateMeshletCount * instanceCount;
				snapshot_.normalConeCandidateTriangles += normalConeCandidateTriangleCount * instanceCount;
			}
		}

		const Snapshot& GetSnapshot() const { return snapshot_; }

		uint64_t GetOneSidedTriangleCount() const
		{
			return snapshot_.trianglesByCullMode[ToCullIndex(MaterialCullMode::Back)]
				+ snapshot_.trianglesByCullMode[ToCullIndex(MaterialCullMode::Front)];
		}

		uint64_t GetEstimatedRasterizerRejectedTriangles() const
		{
			// 実測値ではなく、閉じた一般的なメッシュで表裏が概ね半数という診断用の50%推定です。
			return GetOneSidedTriangleCount() / 2ull;
		}

	private:
		static constexpr size_t ToCullIndex(MaterialCullMode cullMode)
		{
			switch (cullMode)
			{
			case MaterialCullMode::Front: return 1;
			case MaterialCullMode::None: return 2;
			case MaterialCullMode::Back:
			default: return 0;
			}
		}

		static constexpr size_t ToPathIndex(SurfacePath path)
		{
			const size_t index = static_cast<size_t>(path);
			return index < kSurfacePathCount ? index : 0;
		}

		CullingDiagnostics() = default;

		Snapshot snapshot_{};
		MaterialCullMode activeCullMode_ = MaterialCullMode::Back;
		SurfacePath activeSurfacePath_ = SurfacePath::Static;
		bool mainPassActive_ = false;
		bool activeSurfaceValid_ = false;
	};
} // namespace Ken4lowEngine
