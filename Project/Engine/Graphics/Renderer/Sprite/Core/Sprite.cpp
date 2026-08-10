#define NOMINMAX
#include "Sprite.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "ResourceManager.h"
#include "PostEffectManager.h"
#include "GameViewportConstants.h"

#include <cstring>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void Sprite::Initialize(const std::string& filePath)
	{
		dxCommon_ = DirectXCommon::GetInstance();

		filePath_ = filePath;

		TextureManager::GetInstance()->LoadTexture(filePath_);
		textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);

		CreateIndexBuffer();
		CreateMaterialResource();
		CreateVertexBufferResource();
		AdjustTextureSize();
		InitializeReloadProgress();
	}

	/// -------------------------------------------------------------
	///							　更新処理
	/// -------------------------------------------------------------
	void Sprite::Update()
	{
		float left = 0.0f - anchorPoint_.x;
		float right = 1.0f - anchorPoint_.x;
		float top = 0.0f - anchorPoint_.y;
		float bottom = 1.0f - anchorPoint_.y;

		if (isFlipX_)
		{
			left = -left;
			right = -right;
		}

		if (isFlipY_)
		{
			top = -top;
			bottom = -bottom;
		}

		const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(filePath_);

		float tex_left = textureLeftTop_.x / metaData.width;
		float tex_right = (textureLeftTop_.x + textureSize_.x) / metaData.width;
		float tex_top = textureLeftTop_.y / metaData.height;
		float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metaData.height;

		vertexData_[0].position = { left, bottom, 0.0f, 1.0f };
		vertexData_[0].texcoord = { tex_left, tex_bottom };

		vertexData_[1].position = { left, top, 0.0f, 1.0f };
		vertexData_[1].texcoord = { tex_left, tex_top };

		vertexData_[2].position = { right, bottom, 0.0f, 1.0f };
		vertexData_[2].texcoord = { tex_right, tex_bottom };

		vertexData_[3].position = { right, top, 0.0f, 1.0f };
		vertexData_[3].texcoord = { tex_right, tex_top };

		WorldTransform worldTransform;
		worldTransform.scale_ = { size_.x, size_.y, 1.0f };
		worldTransform.rotate_ = { 0.0f, 0.0f, rotation_ };
		worldTransform.translate_ = { position_.x, position_.y, 0.0f };

		Matrix4x4 worldMatrixSprite = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);
		Matrix4x4 viewMatrixSprite = Matrix4x4::MakeIdentity();
		Matrix4x4 projectionMatrixSprite = Matrix4x4::MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(GameViewportConstants::Width), static_cast<float>(GameViewportConstants::Height), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Matrix4x4::Multiply(worldMatrixSprite, Matrix4x4::Multiply(viewMatrixSprite, projectionMatrixSprite));

		transformationMatrixData_.WVP = worldViewProjectionMatrixSprite;
		transformationMatrixData_.World = worldMatrixSprite;
	}

	/// -------------------------------------------------------------
	///						　スプライト描画
	/// -------------------------------------------------------------
	void Sprite::Draw()
	{
		if (!dxCommon_)
		{
			return;
		}

		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		FrameUploadArena& frameUploadArena = dxCommon_->GetFrameUploadArena();

		materialData_.textureIndex = textureIndex_;

		const FrameUploadArena::Allocation vertexAllocation =
			frameUploadArena.Allocate(sizeof(vertexData_), alignof(VertexData));
		const FrameUploadArena::Allocation materialAllocation = frameUploadArena.AllocateConstant(materialData_);
		const FrameUploadArena::Allocation effectAllocation = frameUploadArena.AllocateConstant(effectParamsData_);
		const FrameUploadArena::Allocation transformAllocation = frameUploadArena.AllocateConstant(transformationMatrixData_);
		if (!vertexAllocation.IsValid() || !materialAllocation.IsValid() ||
			!effectAllocation.IsValid() || !transformAllocation.IsValid())
		{
			return;
		}

		std::memcpy(vertexAllocation.cpuAddress, vertexData_.data(), sizeof(vertexData_));

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		vertexBufferView.BufferLocation = vertexAllocation.gpuAddress;
		vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(vertexData_));
		vertexBufferView.StrideInBytes = sizeof(VertexData); // Dynamic頂点も現在Frame専用Upload領域から参照する。

		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		commandList->IASetIndexBuffer(&indexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, materialAllocation.gpuAddress);
		commandList->SetGraphicsRootConstantBufferView(1, effectAllocation.gpuAddress);
		commandList->SetGraphicsRootConstantBufferView(2, transformAllocation.gpuAddress);
		commandList->DrawIndexedInstanced(kNumVertex, 1, 0, 0, 0);
	}

	void Sprite::Finalize()
	{
		if (indexResource && indexData)
		{
			indexResource->Unmap(0, nullptr);
			indexData = nullptr;
		}
		indexResource.Reset();
		textureIndex_ = 0;
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///						テクスチャの変更
	/// -------------------------------------------------------------
	void Sprite::SetTexture(const std::string& filePath)
	{
		filePath_ = filePath;
		TextureManager::GetInstance()->LoadTexture(filePath_);
		textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);
	}

	void Sprite::SetReloadProgress(bool isReloading, float progress)
	{
		effectParamsData_.isReloading = isReloading;
		effectParamsData_.reloadProgress = std::clamp(progress, 0.0f, 1.0f);
	}

	void Sprite::SetCrack(bool enable, float progress)
	{
		effectParamsData_.enableCrack = enable;
		effectParamsData_.crackProgress = std::clamp(progress, 0.0f, 1.0f);
	}

	void Sprite::SetCrackParams(float scale, float thickness, float intensity, const Vector2& hitUV)
	{
		effectParamsData_.crackScale = std::max(1.0f, scale);
		effectParamsData_.crackThickness = std::max(0.0005f, thickness);
		effectParamsData_.crackIntensity = std::max(0.0f, intensity);
		effectParamsData_.crackHitUV = hitUV;
	}

	/// -------------------------------------------------------------
	///	 スプライト用のマテリアルリソースを作成し設定する処理を行う
	/// -------------------------------------------------------------
	void Sprite::CreateMaterialResource()
	{
		materialData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData_.textureIndex = textureIndex_;
		materialData_.uvTransform = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///	  スプライトの頂点バッファリソースと変換行列リソースを生成
	/// -------------------------------------------------------------
	void Sprite::CreateVertexBufferResource()
	{
		vertexData_.fill({});
		transformationMatrixData_.World = Matrix4x4::MakeIdentity();
		transformationMatrixData_.WVP = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///	   スプライトのインデックスバッファを作成および設定する
	/// -------------------------------------------------------------
	void Sprite::CreateIndexBuffer()
	{
		indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kNumVertex);
		indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = sizeof(uint32_t) * kNumVertex;
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
		indexData[0] = 0;
		indexData[1] = 1;
		indexData[2] = 2;
		indexData[3] = 1;
		indexData[4] = 3;
		indexData[5] = 2;
	}

	/// -------------------------------------------------------------
	///				　テクスチャサイズをイメージに合わせる
	/// -------------------------------------------------------------
	void Sprite::AdjustTextureSize()
	{
		const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(filePath_);

		textureSize_.x = static_cast<float>(metaData.width);
		textureSize_.y = static_cast<float>(metaData.height);
		size_ = textureSize_;
	}

	/// -------------------------------------------------------------
	///			　			リロード進捗の初期化処理
	/// -------------------------------------------------------------
	void Sprite::InitializeReloadProgress()
	{
		effectParamsData_.isReloading = false;
		effectParamsData_.reloadProgress = 0.0f;
		effectParamsData_.enableCrack = false;
		effectParamsData_.crackProgress = 0.0f;
		effectParamsData_.crackHitUV = { 0.5f, 0.5f };
		effectParamsData_.crackScale = 8.0f;
		effectParamsData_.crackThickness = 0.03f;
		effectParamsData_.crackIntensity = 1.0f;
	}

} // namespace Ken4lowEngine
