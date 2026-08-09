#include "ParticleMaterial.h"
#include "ResourceManager.h"
#include "DirectXCommon.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				           初期化処理
/// -------------------------------------------------------------
void ParticleMaterial::Initialize()
{
	ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice();

	const UINT stride = Align256(sizeof(MaterialCBData));
	const UINT bufferSize = stride * kSlotCount;

	materialResource_ = ResourceManager::CreateBufferResource(device, bufferSize);
	materialResource_->Map(0, nullptr, &materialDataBase_);
	std::memset(materialDataBase_, 0, bufferSize);

	// 初期値（全スロット同じでOK）
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		auto* m = reinterpret_cast<MaterialCBData*>(reinterpret_cast<uint8_t*>(materialDataBase_) + stride * i);
		m->color = { 1,1,1,1 };
		m->uvTransform = Matrix4x4::MakeIdentity();
		m->drawType = 0;
	}
}

void ParticleMaterial::Finalize()
{
	if (materialResource_)
	{
		materialResource_->Unmap(0, nullptr);
		materialDataBase_ = nullptr;
		materialData_ = nullptr;
		materialResource_.Reset();
	}
}

/// -------------------------------------------------------------
///				           　更新処理
/// -------------------------------------------------------------
void ParticleMaterial::Update()
{
	// マテリアルデータがある場合
	if (materialData_)
	{
		materialData_->color = this->materialData_->color;			   // 色
		materialData_->uvTransform = this->materialData_->uvTransform; // UV変換行列
	}
}

/// -------------------------------------------------------------
///				         パイプラインの設定
/// -------------------------------------------------------------
void ParticleMaterial::SetPipeline(UINT rootParameterIndex, uint32_t slot) const
{
	ID3D12GraphicsCommandList* commandList = DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();

	// マテリアルのリソースがある場合
	if (materialResource_)
	{
		const UINT stride = Align256(sizeof(MaterialCBData));
		const uint32_t s = slot % kSlotCount;
		commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, materialResource_->GetGPUVirtualAddress() + stride * s);
	}
}


} // namespace Ken4lowEngine
