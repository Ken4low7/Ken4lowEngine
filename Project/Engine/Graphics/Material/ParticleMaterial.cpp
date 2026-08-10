#include "ParticleMaterial.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				           初期化処理
/// -------------------------------------------------------------
void ParticleMaterial::Initialize()
{
	for (MaterialCBData& material : materialSlots_)
	{
		material = {};
		material.color = { 1, 1, 1, 1 };
		material.uvTransform = Matrix4x4::MakeIdentity();
		material.drawType = 0;
	}
}

void ParticleMaterial::Finalize()
{
	materialSlots_ = {};
}

/// -------------------------------------------------------------
///				           　更新処理
/// -------------------------------------------------------------
void ParticleMaterial::Update()
{
	// CPUステージング値はSetDrawType/GetSlotData経由で直接更新し、描画時にFrameUploadArenaへ転送する。
}

/// -------------------------------------------------------------
///				         パイプラインの設定
/// -------------------------------------------------------------
void ParticleMaterial::SetPipeline(UINT rootParameterIndex, uint32_t slot) const
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandManager()->GetCommandList();
	const MaterialCBData& material = materialSlots_[slot % kSlotCount];
	const FrameUploadArena::Allocation allocation = dxCommon->GetFrameUploadArena().AllocateConstant(material);
	if (!allocation.IsValid())
	{
		return;
	}

	commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, allocation.gpuAddress);
}


} // namespace Ken4lowEngine
