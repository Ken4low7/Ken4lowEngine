#include "GpuParticleRenderer.h"
#include "DirectXCommon.h"
#include "GpuParticleSpritePipeline.h"
#include "GpuParticleBuffers.h"
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

	/// -------------------------------------------------------------
	///				　　　	初期化処理
	/// -------------------------------------------------------------
	void GpuParticleRenderer::Initialize(GpuParticleSpritePipeline* pipeline, GpuParticleBuffers* buffers)
	{
		TextureManager::GetInstance()->LoadTexture(textureFilePath_);

		// 引数でパイプラインとバッファのポインタを受け取ってメンバ変数に記録する
		gpuParticlePipeline_ = pipeline;
		gpuParticleBuffers_ = buffers;

		// パーティクルメッシュの生成と初期化
		particleMesh_ = std::make_unique<ParticleMesh>();
		particleMesh_->Initialize();

		// パーティクルマテリアルの生成と初期化
		particleMaterial_ = std::make_unique<ParticleMaterial>();
		particleMaterial_->Initialize();

		// メッシュパーティクル用パイプライン
		gpuParticleMeshPipeline_ = std::make_unique<GpuParticleMeshPipeline>();
		gpuParticleMeshPipeline_->Initialize();
	}

	/// -------------------------------------------------------------
	///				　　　			描画処理
	/// -------------------------------------------------------------
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

		if (textureFilePath_.size() <= kMeshTexturePrefix.size())
		{
			return false;
		}

		const std::string_view pathView(textureFilePath_);
		if (pathView.substr(0, kMeshTexturePrefix.size()) != kMeshTexturePrefix)
		{
			return false;
		}

		const std::string_view numberView = pathView.substr(kMeshTexturePrefix.size());
		uint32_t parsed = 0;
		const auto* begin = numberView.data();
		const auto* end = numberView.data() + numberView.size();
		const auto result = std::from_chars(begin, end, parsed);

		if (result.ec != std::errc{} || result.ptr != end)
		{
			return false;
		}

		outMeshId = parsed;
		return true;
	}

	void GpuParticleRenderer::DrawSprite(UINT instanceCount, uint32_t slot)
	{
		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

		PostEffectManager::GetInstance()->BindSceneRenderTarget();

		// パイプラインの設定
		commandList->SetGraphicsRootSignature(gpuParticlePipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticlePipeline_->GetGfxPSO());

		// メッシュ（VB/IB）セット
		const auto& vbView = particleMesh_->GetVertexBufferView();
		commandList->IASetVertexBuffers(0, 1, &vbView);

		// ディスクリプタヒープの設定
		SRVManager::GetInstance()->PreDraw();

		// PerView を CBV として設定
		commandList->SetGraphicsRootConstantBufferView(0, gpuParticleBuffers_->GetPerViewBuffer()->GetGPUVirtualAddress());

		// パーティクルバッファのSRVをセット
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());

		// マテリアル設定
		particleMaterial_->SetPipeline(2, slot);

		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 3, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

		// インスタンス数分描画
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
		if (!mesh || !gpuParticleMeshPipeline_ || !particleMaterial_)
		{
			return;
		}

		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

		PostEffectManager::GetInstance()->BindSceneRenderTarget();

		commandList->SetGraphicsRootSignature(gpuParticleMeshPipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticleMeshPipeline_->GetGfxPSO());

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &mesh->vbv);

		if (mesh->indexCount > 0)
		{
			commandList->IASetIndexBuffer(&mesh->ibv);
		}

		SRVManager::GetInstance()->PreDraw();

		// b0 : PerView
		commandList->SetGraphicsRootConstantBufferView(0, gpuParticleBuffers_->GetPerViewBuffer()->GetGPUVirtualAddress());

		// t0 : Particle SRV
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());

		// b1 : Material
		particleMaterial_->SetPipeline(2, slot);

		// t0 : Texture
		std::string texturePath = mesh->textureFilePath.empty() ? kFallbackParticleTexture : mesh->textureFilePath;
		TextureManager::GetInstance()->LoadTexture(texturePath);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(
			commandList,
			3,
			TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));

		if (mesh->indexCount > 0)
		{
			commandList->DrawIndexedInstanced(mesh->indexCount, instanceCount, 0, 0, 0);
		}
	}

	void GpuParticleRenderer::SetTextureFilePath(const std::string& path)
	{
		textureFilePath_ = path;

		uint32_t meshId = 0;
		if (TryGetMeshIdFromTexturePath(meshId))
		{
			return;
		}

		TextureManager::GetInstance()->LoadTexture(textureFilePath_); // 念のためロード（キャッシュされる想定）
	}

	void GpuParticleRenderer::SetDrawType(uint32_t drawType, uint32_t slot)
	{
		if (particleMaterial_) { particleMaterial_->SetDrawType(drawType, slot); }
	}
} // namespace Ken4lowEngine
