#pragma once
#include <DX12Include.h>
#include "ParticleMesh.h"
#include "ParticleMaterial.h"
#include "GpuParticleMeshPipeline.h"
#include "BlendModeType.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class GpuParticleSpritePipeline;
class GpuParticleBuffers;

class GpuParticleRenderer
{
public:
	struct GpuDrivenStatistics
	{
		uint64_t drawRequests = 0;
		uint64_t compactionDispatches = 0;
		uint64_t compactionParticleScans = 0;
		uint64_t alphaSortGroups = 0;
		uint64_t alphaSortDispatches = 0;
		uint64_t indirectDraws = 0;
	};

	struct GpuTimingMetric
	{
		float lastMs = 0.0f;
		float averageMs = 0.0f;
		float maxMs = 0.0f;
		uint64_t sampleCount = 0;
	};

	struct GpuDrivenGpuTimings
	{
		GpuTimingMetric compaction{};
		GpuTimingMetric alphaSort{};
		GpuTimingMetric graphics{};
		GpuTimingMetric total{};
	};

	void Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers);

	/// Manager互換の引数は残しつつ、Phase14ではGPU Compactionが実Instance数をIndirect Argsへ書き込む。
	void Draw(UINT instanceCount, uint32_t slot = 0);

	/// Stress/Profiler側が区間単位でGPU Driven workloadを比較できるようCPU同期不要の発行統計だけを公開する。
	void ResetGpuDrivenStatistics() { gpuDrivenStatistics_ = {}; }
	const GpuDrivenStatistics& GetGpuDrivenStatistics() const { return gpuDrivenStatistics_; }
	const GpuDrivenGpuTimings& GetGpuDrivenGpuTimings() const { return gpuDrivenGpuTimings_; }
	bool IsGpuTimingAvailable() const { return gpuTimingAvailable_; }

	/// Editor診断からManagerのprivate ownershipを崩さずProfiler snapshotへ到達するための参照です。
	static GpuParticleRenderer* GetActiveRenderer() { return activeRenderer_; }

public:
	void SetTextureFilePath(const std::string& path);
	void SetDrawType(uint32_t type, uint32_t slot);

private:
	bool TryGetMeshIdFromTexturePath(uint32_t& outMeshId) const;
	void DrawSprite(uint32_t slot);
	void DrawMesh(uint32_t slot, uint32_t meshId);
	void CreateGpuDrivenPipeline();
	void CreateIndirectCommandSignatures();
	bool BuildVisibleParticleList(uint32_t primitiveCount, bool indexed);
	void SortVisibleParticlesByDepth();

	void InitializeGpuTiming();
	void PrepareGpuTimingFrame();
	void CollectGpuTiming(uint32_t frameIndex);
	void BeginGpuTimingSample();
	void WriteGpuTimingPoint(uint32_t pointIndex);
	void EndGpuTimingSample();
	void CancelGpuTimingSample();
	void UpdateGpuTimingMetric(GpuTimingMetric& metric, float elapsedMs);
	uint32_t GetGpuTimingFrameBaseQuery(uint32_t frameIndex) const;
	uint32_t GetGpuTimingSampleBaseQuery(uint32_t frameIndex, uint32_t sampleIndex) const;

private:
	static constexpr uint32_t kGpuTimingQueriesPerSample = 4;
	static constexpr uint32_t kGpuTimingMaxSamplesPerFrame = 256;
	static constexpr uint32_t kInvalidGpuTimingSample = UINT32_MAX;

	struct GpuTimingFrameState
	{
		uint32_t sampleCount = 0;
		bool pendingResolve = false;
		std::array<uint8_t, kGpuTimingMaxSamplesPerFrame> alphaSamples{};
	};

	GpuParticleSpritePipeline* gpuParticlePipeline_ = nullptr;
	GpuParticleBuffers* gpuParticleBuffers_ = nullptr;
	std::unique_ptr<ParticleMesh> particleMesh_;
	std::unique_ptr<ParticleMaterial> particleMaterial_;
	std::unique_ptr<GpuParticleMeshPipeline> gpuParticleMeshPipeline_;

	// CompactionとAlpha Sortを同じUAV契約で連結し、CPU readbackなしでIndirect Drawへ渡す。
	ComPtr<ID3D12RootSignature> compactionRootSignature_;
	ComPtr<ID3D12PipelineState> compactionPipelineState_;
	ComPtr<ID3D12PipelineState> depthSortPipelineState_;
	ComPtr<ID3D12CommandSignature> drawCommandSignature_;
	ComPtr<ID3D12CommandSignature> drawIndexedCommandSignature_;
	bool gpuDrivenBuffersReadable_ = false;
	uint32_t shaderRenderGroup_ = 0;
	GpuDrivenStatistics gpuDrivenStatistics_{};

	// Timestamp readbackはFrameResource再利用時だけ読むため、Profilerを有効にしてもGPU待機を追加しない。
	ComPtr<ID3D12QueryHeap> gpuTimestampHeap_;
	ComPtr<ID3D12Resource> gpuTimestampReadback_;
	std::vector<GpuTimingFrameState> gpuTimingFrameStates_{};
	GpuDrivenGpuTimings gpuDrivenGpuTimings_{};
	uint64_t gpuTimestampFrequency_ = 0;
	uint64_t currentGpuTimingFrameFenceValue_ = UINT64_MAX;
	uint32_t gpuTimingFrameResourceCount_ = 0;
	uint32_t currentGpuTimingFrameIndex_ = UINT32_MAX;
	uint32_t activeGpuTimingSample_ = kInvalidGpuTimingSample;
	bool gpuTimingAvailable_ = false;

	std::string textureFilePath_ = "Effects/circle2.dds";
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;

	inline static GpuParticleRenderer* activeRenderer_ = nullptr;
};

} // namespace Ken4lowEngine
