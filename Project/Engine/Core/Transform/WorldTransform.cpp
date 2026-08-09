#include "WorldTransform.h"
#include <DirectXCommon.h>
#include <CameraManager.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                 ワールド変換行列初期化処理
	/// -------------------------------------------------------------
	void WorldTransform::Initialize()
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		const uint32_t frameCount = dxCommon->GetCommandManager()->GetFrameResourceCount();
		transformationBuffers_.Initialize(dxCommon->GetDevice(), frameCount);

		transformationData_.World = Matrix4x4::MakeIdentity();
		transformationData_.WVP = Matrix4x4::MakeIdentity();
		transformationData_.WorldInversedTranspose = Matrix4x4::MakeIdentity();
		transformationBuffers_.WriteAll(transformationData_); // 切替直後のどのFrameResourceでも同じ初期行列を参照できるようにする。

		matWorld_ = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///                 ワールド変換行列更新処理
	/// -------------------------------------------------------------
	void WorldTransform::Update()
	{
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);

		if (parent_)
		{
			worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->matWorld_);
		}

		worldRotate_ = parent_ ? parent_->worldRotate_ + rotate_ : rotate_;
		worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjection);

		matWorld_ = worldMatrix;
		transformationData_.WVP = worldViewProjectionMatrix;
		transformationData_.World = worldMatrix;
		transformationData_.WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}

	/// -------------------------------------------------------------
	///                 合成済みワールド行列の反映処理
	/// -------------------------------------------------------------
	void WorldTransform::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix)
	{
		matWorld_ = worldMatrix;
		worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, viewProjection);

		transformationData_.WVP = worldViewProjectionMatrix;
		transformationData_.World = worldMatrix;
		transformationData_.WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
	}

	/// -------------------------------------------------------------
	///                 パイプライン設定処理
	/// -------------------------------------------------------------
	void WorldTransform::SetPipeline(UINT rootParameterIndex)
	{
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		auto* commandManager = dxCommon->GetCommandManager();
		const uint32_t frameIndex = commandManager->GetCurrentFrameIndex();
		transformationBuffers_.WriteFrame(frameIndex, transformationData_);

		auto commandList = commandManager->GetCommandList();
		const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = transformationBuffers_.GetGpuAddress(frameIndex);
		if (gpuAddress != 0)
		{
			commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, gpuAddress);
		}
	}

} // namespace Ken4lowEngine
