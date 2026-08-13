#include "GpuParticleRenderer.h"
#include "DirectXCommon.h"
#include "GpuParticleSpritePipeline.h"
#include "GpuParticleBuffers.h"
#include "GpuParticleEmitterData.h"
#include "GpuParticleManager.h"
#include "SRVManager.h"
#include "PostEffectManager.h"
#include <TextureManager.h>

#include <charconv>
#include <string_view>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr std::string_view kMeshTexturePrefix = "Mesh:";
		constexpr const char* kFallbackParticleTexture = "Effects/white.dds";
	}

	void GpuParticleRenderer::Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers)
	{
		TextureManager::GetInstance()->LoadTexture(textureFilePath_);
		gpuParticlePipeline_ = pipeline;
		gpuParticleBuffers_ = buffers;
		particleMesh_ = std::make_unique<ParticleMesh>();
		particleMesh_->Initialize();
		particleMaterial_ = std::make_unique<ParticleMaterial>();
		particleMaterial_->Initialize();
		gpuParticleMeshPipeline_ = std::make_unique<GpuParticleMeshPipeline>();
		gpuParticleMeshPipeline_->Initialize();
	}

	void GpuParticleRenderer::Draw(UINT instanceCount, uint32_t slot)
	{
		uint32_t meshId = 0;
		if (TryGetMeshIdFromTexturePath(meshId))
		{
			DrawMesh(instanceCount, slot, meshId);
			return;
		}
		DrawSprite(instanceCount, slot);
	}

	bool GpuParticleRenderer::TryGetMeshIdFromTexturePath(uint32_t& outMeshId) const
	{
		outMeshId = 0;
		if (textureFilePath_.size() <= kMeshTexturePrefix.size()) return false;

		const std::string_view pathView(textureFilePath_);
		if (pathView.substr(0, kMeshTexturePrefix.size()) != kMeshTexturePrefix) return false;

		const std::string_view numberView = pathView.substr(kMeshTexturePrefix.size());
		uint32_t parsed = 0;
		const auto* begin = numberView.data();
		const auto* end = numberView.data() + numberView.size();
		const auto result = std::from_chars(begin, end, parsed);
		if (result.ec != std::errc{} || result.ptr != end) return false;

		outMeshId = parsed;
		return true;
	}

	void GpuParticleRenderer::DrawSprite(UINT instanceCount, uint32_t slot)
	{
		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		commandList->SetGraphicsRootSignature(gpuParticlePipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticlePipeline_->GetGfxPSO(blendMode_));

		const auto& vbView = particleMesh_->GetVertexBufferView();
		commandList->IASetVertexBuffers(0, 1, &vbView);
		SRVManager::GetInstance()->PreDraw();

		const D3D12_GPU_VIRTUAL_ADDRESS perViewAddress = gpuParticleBuffers_->GetPerViewCBAddress();
		if (perViewAddress == 0) return;
		commandList->SetGraphicsRootConstantBufferView(0, perViewAddress); // PerViewは現在FrameのUpload Arenaに置き、前Frameと安全に並行させる。
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());
		particleMaterial_->SetPipeline(2, slot);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 3, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

		if (particleMesh_->HasIndex())
		{
			const auto& ibView = particleMesh_->GetIndexBufferView();
			commandList->IASetIndexBuffer(&ibView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(static_cast<UINT>(ibView.SizeInBytes / sizeof(uint32_t)), instanceCount, 0, 0, 0);
		}
		else
		{
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawInstanced(static_cast<UINT>(vbView.SizeInBytes / vbView.StrideInBytes), instanceCount, 0, 0);
		}
	}

	void GpuParticleRenderer::DrawMesh(UINT instanceCount, uint32_t slot, uint32_t meshId)
	{
		const MeshParticleAsset* mesh = GpuParticleManager::GetInstance()->FindMeshAsset(meshId);
		if (!mesh || !gpuParticleMeshPipeline_ || !particleMaterial_) return;

		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		commandList->SetGraphicsRootSignature(gpuParticleMeshPipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticleMeshPipeline_->GetGfxPSO(blendMode_));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &mesh->vbv);
		if (mesh->indexCount > 0) commandList->IASetIndexBuffer(&mesh->ibv);
		SRVManager::GetInstance()->PreDraw();

		const D3D12_GPU_VIRTUAL_ADDRESS perViewAddress = gpuParticleBuffers_->GetPerViewCBAddress();
		if (perViewAddress == 0) return;
		commandList->SetGraphicsRootConstantBufferView(0, perViewAddress);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());
		particleMaterial_->SetPipeline(2, slot);

		std::string texturePath = mesh->textureFilePath.empty() ? kFallbackParticleTexture : mesh->textureFilePath;
		TextureManager::GetInstance()->LoadTexture(texturePath);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(
			commandList,
			3,
			TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

		if (mesh->indexCount > 0) commandList->DrawIndexedInstanced(mesh->indexCount, instanceCount, 0, 0, 0);
	}

	void GpuParticleRenderer::SetTextureFilePath(const std::string& path)
	{
		textureFilePath_ = path;
		uint32_t meshId = 0;
		if (TryGetMeshIdFromTexturePath(meshId)) return;
		TextureManager::GetInstance()->LoadTexture(textureFilePath_); // 念のためロード（キャッシュされる想定）
	}

	void GpuParticleRenderer::SetDrawType(uint32_t drawType, uint32_t slot)
	{
		blendMode_ = UnpackGpuParticleBlendMode(drawType);
		const uint32_t materialDrawType = UnpackGpuParticleMaterialDrawType(drawType);
		const bool hasAuthoredBlendTag = (drawType & kGpuParticleBlendTagMask) != 0u;
		const uint32_t shaderRenderGroup = hasAuthoredBlendTag
			? BuildGpuParticleRenderGroup(textureFilePath_, materialDrawType, blendMode_)
			: materialDrawType;

		if (particleMaterial_)
		{
			// Legacy Emitterは従来type、Authoring EmitterだけTexture/Mesh+Blend hashを使い互換性と多Material描画を両立する。
			particleMaterial_->SetDrawType(shaderRenderGroup, slot);
		}
	}
} // namespace Ken4lowEngine
