#include "GpuParticleRenderer.h"
#include "DirectXCommon.h"
#include "GpuParticleSpritePipeline.h"
#include "GpuParticleBuffers.h"
#include "GpuParticleEmitterData.h"
#include "GpuParticleManager.h"
#include "GpuParticleShaderManifest.h"
#include "SRVManager.h"
#include "UAVManager.h"
#include "PostEffectManager.h"
#include "ShaderCompiler.h"
#include <TextureManager.h>

#include <cassert>
#include <charconv>
#include <string_view>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr std::string_view kMeshTexturePrefix = "Mesh:";
		constexpr const char* kFallbackParticleTexture = "Effects/white.dds";
		constexpr UINT kCompactionThreadCount = 256;
		constexpr UINT kIndirectArgumentStride = sizeof(uint32_t) * 5;
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
		CreateGpuDrivenPipeline();
		CreateIndirectCommandSignatures();
	}

	void GpuParticleRenderer::Draw(UINT instanceCount, uint32_t slot)
	{
		// Phase14ではinstanceCountをCPUで決めず、Compaction CSがIndirect ArgsのInstanceCountを生成する。
		(void)instanceCount;

		uint32_t meshId = 0;
		if (TryGetMeshIdFromTexturePath(meshId))
		{
			DrawMesh(slot, meshId);
			return;
		}
		DrawSprite(slot);
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

	void GpuParticleRenderer::CreateGpuDrivenPipeline()
	{
		auto* dxCommon = DirectXCommon::GetInstance();
		auto* device = dxCommon->GetDevice();

		D3D12_DESCRIPTOR_RANGE uavRanges[3]{};
		for (UINT index = 0; index < 3; ++index)
		{
			uavRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			uavRanges[index].NumDescriptors = 1;
			uavRanges[index].BaseShaderRegister = index;
			uavRanges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}

		D3D12_ROOT_PARAMETER params[4]{};
		params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		params[0].Descriptor.ShaderRegister = 0;
		params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		for (UINT index = 0; index < 3; ++index)
		{
			params[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			params[index + 1].DescriptorTable.NumDescriptorRanges = 1;
			params[index + 1].DescriptorTable.pDescriptorRanges = &uavRanges[index];
			params[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		D3D12_ROOT_SIGNATURE_DESC rootDesc{};
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		rootDesc.pParameters = params;
		rootDesc.NumParameters = _countof(params);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signature,
			&error);
		assert(SUCCEEDED(hr));

		hr = device->CreateRootSignature(
			0,
			signature->GetBufferPointer(),
			signature->GetBufferSize(),
			IID_PPV_ARGS(&compactionRootSignature_));
		assert(SUCCEEDED(hr));

		const ShaderDescriptor& compactShader =
			GpuParticleShaderManifest::GetCompute(GpuParticleComputeShaderId::CompactCS);
		ComPtr<IDxcBlob> compactCs = ShaderCompiler::CompileShader(compactShader, dxCommon->GetDXCCompilerManager());
		assert(compactCs != nullptr);

		D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = compactionRootSignature_.Get();
		pipelineDesc.CS = { compactCs->GetBufferPointer(), compactCs->GetBufferSize() };
		hr = device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&compactionPipelineState_));
		assert(SUCCEEDED(hr));

		compactionRootSignature_->SetName(L"GpuParticleRenderer_CompactionRootSignature");
		compactionPipelineState_->SetName(L"GpuParticleRenderer_CompactionPSO");
	}

	void GpuParticleRenderer::CreateIndirectCommandSignatures()
	{
		auto* device = DirectXCommon::GetInstance()->GetDevice();

		D3D12_INDIRECT_ARGUMENT_DESC drawArgument{};
		drawArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		D3D12_COMMAND_SIGNATURE_DESC drawSignatureDesc{};
		drawSignatureDesc.ByteStride = kIndirectArgumentStride;
		drawSignatureDesc.NumArgumentDescs = 1;
		drawSignatureDesc.pArgumentDescs = &drawArgument;
		HRESULT hr = device->CreateCommandSignature(&drawSignatureDesc, nullptr, IID_PPV_ARGS(&drawCommandSignature_));
		assert(SUCCEEDED(hr));

		D3D12_INDIRECT_ARGUMENT_DESC indexedArgument{};
		indexedArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		D3D12_COMMAND_SIGNATURE_DESC indexedSignatureDesc{};
		indexedSignatureDesc.ByteStride = kIndirectArgumentStride;
		indexedSignatureDesc.NumArgumentDescs = 1;
		indexedSignatureDesc.pArgumentDescs = &indexedArgument;
		hr = device->CreateCommandSignature(&indexedSignatureDesc, nullptr, IID_PPV_ARGS(&drawIndexedCommandSignature_));
		assert(SUCCEEDED(hr));
	}

	bool GpuParticleRenderer::BuildVisibleParticleList(uint32_t primitiveCount, bool indexed)
	{
		if (!gpuParticleBuffers_ || !compactionPipelineState_ || primitiveCount == 0) return false;

		ID3D12Resource* particleBuffer = gpuParticleBuffers_->GetParticleBuffer();
		ID3D12Resource* visibleBuffer = gpuParticleBuffers_->GetVisibleParticleIndexBuffer();
		ID3D12Resource* indirectBuffer = gpuParticleBuffers_->GetIndirectDrawArgsBuffer();
		if (!particleBuffer || !visibleBuffer || !indirectBuffer) return false;

		// CB確保失敗時にResource StateだけUAVへ残さないよう、GPU state変更より前に確保する。
		const D3D12_GPU_VIRTUAL_ADDRESS drawCbAddress = gpuParticleBuffers_->GetGpuDrivenDrawCBAddress(
			shaderRenderGroup_, primitiveCount, indexed);
		if (drawCbAddress == 0) return false;

		auto* dxCommon = DirectXCommon::GetInstance();
		auto* commandList = dxCommon->GetCommandManager()->GetCommandList();
		auto* uavManager = UAVManager::GetInstance();

		if (gpuDrivenBuffersReadable_)
		{
			dxCommon->ResourceTransition(visibleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			dxCommon->ResourceTransition(indirectBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}
		dxCommon->ResourceTransition(particleBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		uavManager->PreDispatch();
		const UINT clearValues[4] = { 0u, 0u, 0u, 0u };
		commandList->ClearUnorderedAccessViewUint(
			uavManager->GetGPUDescriptorHandle(gpuParticleBuffers_->GetIndirectDrawArgsUavIndex()),
			uavManager->GetCPUDescriptorHandle(gpuParticleBuffers_->GetIndirectDrawArgsUavIndex()),
			indirectBuffer,
			clearValues,
			0,
			nullptr);

		// Clear UAVの書き込みをCompaction CSのInterlockedAddより前に確定させる。
		D3D12_RESOURCE_BARRIER clearBarrier{};
		clearBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		clearBarrier.UAV.pResource = indirectBuffer;
		commandList->ResourceBarrier(1, &clearBarrier);

		commandList->SetComputeRootSignature(compactionRootSignature_.Get());
		commandList->SetPipelineState(compactionPipelineState_.Get());
		commandList->SetComputeRootConstantBufferView(0, drawCbAddress);
		commandList->SetComputeRootDescriptorTable(1, uavManager->GetGPUDescriptorHandle(gpuParticleBuffers_->GetParticleUavIndex()));
		commandList->SetComputeRootDescriptorTable(2, uavManager->GetGPUDescriptorHandle(gpuParticleBuffers_->GetVisibleParticleIndexUavIndex()));
		commandList->SetComputeRootDescriptorTable(3, uavManager->GetGPUDescriptorHandle(gpuParticleBuffers_->GetIndirectDrawArgsUavIndex()));

		const UINT groupCountX = (GpuParticleBuffers::GetMaxParticles() + kCompactionThreadCount - 1) / kCompactionThreadCount;
		commandList->Dispatch(groupCountX, 1, 1);

		// TransitionがCompaction書き込み完了を可視化し、直後のVS/ExecuteIndirectから同じ結果を安全に読む。
		dxCommon->ResourceTransition(particleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		dxCommon->ResourceTransition(visibleBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		dxCommon->ResourceTransition(indirectBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		gpuDrivenBuffersReadable_ = true;
		return true;
	}

	void GpuParticleRenderer::DrawSprite(uint32_t slot)
	{
		const auto& vbView = particleMesh_->GetVertexBufferView();
		const bool indexed = particleMesh_->HasIndex();
		const uint32_t primitiveCount = indexed
			? static_cast<uint32_t>(particleMesh_->GetIndexBufferView().SizeInBytes / sizeof(uint32_t))
			: static_cast<uint32_t>(vbView.SizeInBytes / vbView.StrideInBytes);
		if (!BuildVisibleParticleList(primitiveCount, indexed)) return;

		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		commandList->SetGraphicsRootSignature(gpuParticlePipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticlePipeline_->GetGfxPSO(blendMode_));
		commandList->IASetVertexBuffers(0, 1, &vbView);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (indexed)
		{
			const auto& ibView = particleMesh_->GetIndexBufferView();
			commandList->IASetIndexBuffer(&ibView);
		}

		SRVManager::GetInstance()->PreDraw();
		const D3D12_GPU_VIRTUAL_ADDRESS perViewAddress = gpuParticleBuffers_->GetPerViewCBAddress();
		if (perViewAddress == 0) return;
		commandList->SetGraphicsRootConstantBufferView(0, perViewAddress);
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(1, gpuParticleBuffers_->GetParticleSrvIndex());
		particleMaterial_->SetPipeline(2, slot);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 3, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(4, gpuParticleBuffers_->GetVisibleParticleIndexSrvIndex());

		ID3D12Resource* args = gpuParticleBuffers_->GetIndirectDrawArgsBuffer();
		if (indexed)
		{
			commandList->ExecuteIndirect(drawIndexedCommandSignature_.Get(), 1, args, 0, nullptr, 0);
		}
		else
		{
			commandList->ExecuteIndirect(drawCommandSignature_.Get(), 1, args, 0, nullptr, 0);
		}
	}

	void GpuParticleRenderer::DrawMesh(uint32_t slot, uint32_t meshId)
	{
		const MeshParticleAsset* mesh = GpuParticleManager::GetInstance()->FindMeshAsset(meshId);
		if (!mesh || !gpuParticleMeshPipeline_ || !particleMaterial_ || mesh->indexCount == 0) return;
		if (!BuildVisibleParticleList(mesh->indexCount, true)) return;

		auto* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
		PostEffectManager::GetInstance()->BindSceneRenderTarget();
		commandList->SetGraphicsRootSignature(gpuParticleMeshPipeline_->GetGfxRootSignature());
		commandList->SetPipelineState(gpuParticleMeshPipeline_->GetGfxPSO(blendMode_));
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &mesh->vbv);
		commandList->IASetIndexBuffer(&mesh->ibv);
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
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(4, gpuParticleBuffers_->GetVisibleParticleIndexSrvIndex());

		commandList->ExecuteIndirect(
			drawIndexedCommandSignature_.Get(),
			1,
			gpuParticleBuffers_->GetIndirectDrawArgsBuffer(),
			0,
			nullptr,
			0);
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
		shaderRenderGroup_ = hasAuthoredBlendTag
			? BuildGpuParticleRenderGroup(textureFilePath_, materialDrawType, blendMode_)
			: materialDrawType;

		if (particleMaterial_)
		{
			particleMaterial_->SetDrawType(shaderRenderGroup_, slot);
		}
	}
} // namespace Ken4lowEngine
