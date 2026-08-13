#pragma once
#include <DX12Include.h>
#include "ParticleMesh.h"
#include "ParticleMaterial.h"
#include "GpuParticleMeshPipeline.h"
#include "BlendModeType.h"

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

	void Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers);

	/// Manager互換の引数は残しつつ、Phase14ではGPU Compactionが実Instance数をIndirect Argsへ書き込む。
	void Draw(UINT instanceCount, uint32_t slot = 0);

	/// Stress/Profiler側が区間単位でGPU Driven workloadを比較できるようCPU同期不要の発行統計だけを公開する。
	void ResetGpuDrivenStatistics() { gpuDrivenStatistics_ = {}; }
	const GpuDrivenStatistics& GetGpuDrivenStatistics() const { return gpuDrivenStatistics_; }

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

private:
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

	std::string textureFilePath_ = "Effects/circle2.dds";
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;
};

} // namespace Ken4lowEngine
