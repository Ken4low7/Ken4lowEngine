#define NOMINMAX
#include "Sprite.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "ResourceManager.h"
#include "PostEffectManager.h"
#include "GameViewportConstants.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							初期化処理
	/// -------------------------------------------------------------
	void Sprite::Initialize(const std::string& filePath)
	{
		dxCommon_ = DirectXCommon::GetInstance();

		filePath_ = filePath;

		// テクスチャの読み込み
		TextureManager::GetInstance()->LoadTexture(filePath_);

		// テクスチャのSRV用GPUハンドルを取得
		textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);

		// スプライトのインデックスバッファを作成および設定する
		CreateIndexBuffer();

		// スプライト用のマテリアルリソースを作成し設定する処理を行う
		CreateMaterialResource();

		// スプライトの頂点バッファリソースと変換行列リソースを生成
		CreateVertexBufferResource();

		// テクスチャサイズに合わせる
		AdjustTextureSize();

		// リロード進捗の初期化処理
		InitializeReloadProgress();
	}

	/// -------------------------------------------------------------
	///							　更新処理
	/// -------------------------------------------------------------
	void Sprite::Update()
	{
		// アンカーポイント
		float left = 0.0f - anchorPoint_.x;   // 左端
		float right = 1.0f - anchorPoint_.x;  // 右端
		float top = 0.0f - anchorPoint_.y;	  // 上端
		float bottom = 1.0f - anchorPoint_.y; // 下端

		// 左右反転
		if (isFlipX_)
		{
			left = -left;   // 左端
			right = -right; // 右端
		}

		// 上下反転
		if (isFlipY_)
		{
			top = -top;		  // 上端
			bottom = -bottom; // 下端
		}

		// メタデータ取得
		const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(filePath_);

		// テクスチャ範囲指定
		float tex_left = textureLeftTop_.x / metaData.width;					   // テクスチャ左端
		float tex_right = (textureLeftTop_.x + textureSize_.x) / metaData.width;   // テクスチャ右端
		float tex_top = textureLeftTop_.y / metaData.height;					   // テクスチャ上端
		float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metaData.height; // テクスチャ下端

		/// ---------- 頂点データ設定 ---------- ///

		// 左上
		vertexData[0].position = { left, bottom, 0.0f, 1.0f };
		vertexData[0].texcoord = { tex_left, tex_bottom };

		// 左下
		vertexData[1].position = { left, top, 0.0f, 1.0f };
		vertexData[1].texcoord = { tex_left, tex_top };

		// 右下
		vertexData[2].position = { right, bottom, 0.0f, 1.0f };
		vertexData[2].texcoord = { tex_right, tex_bottom };

		// 右上
		vertexData[3].position = { right, top, 0.0f, 1.0f };
		vertexData[3].texcoord = { tex_right, tex_top };

		// ワールド行列の計算
		WorldTransform worldTransform;
		worldTransform.scale_ = { size_.x, size_.y, 1.0f };
		worldTransform.rotate_ = { 0.0f, 0.0f, rotation_ };
		worldTransform.translate_ = { position_.x, position_.y, 0.0f };

		// ワールド行列の計算
		Matrix4x4 worldMatrixSprite = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

		// スプライトUIは固定GameViewportRenderTarget(1920x1080)基準で射影し、最後に画面サイズへ拡縮する。
		Matrix4x4 viewMatrixSprite = Matrix4x4::MakeIdentity();
		Matrix4x4 projectionMatrixSprite = Matrix4x4::MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(GameViewportConstants::Width), static_cast<float>(GameViewportConstants::Height), 0.0f, 100.0f);
		Matrix4x4 worldViewProjectionMatrixSprite = Matrix4x4::Multiply(worldMatrixSprite, Matrix4x4::Multiply(viewMatrixSprite, projectionMatrixSprite));

		// 座標変換行列を更新
		transformationMatrixData->WVP = worldViewProjectionMatrixSprite;
		transformationMatrixData->World = worldMatrixSprite;
	}

	/// -------------------------------------------------------------
	///						　スプライト描画
	/// -------------------------------------------------------------
	void Sprite::Draw()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();

		// スプライトが使うテクスチャ番号をシェーダーに渡す
		materialData->textureIndex = textureIndex_;

		commandList->IASetVertexBuffers(0, 1, &vertexBufferView); // スプライト用VBV
		commandList->IASetIndexBuffer(&indexBufferView); // IBVの設定
		commandList->SetGraphicsRootConstantBufferView(0, materialResource.Get()->GetGPUVirtualAddress()); // マテリアルリソースの設定
		commandList->SetGraphicsRootConstantBufferView(1, effectParamsResource.Get()->GetGPUVirtualAddress()); // リロード進捗リソースの設定
		commandList->SetGraphicsRootConstantBufferView(2, transformationMatrixResource.Get()->GetGPUVirtualAddress()); // 座標変換行列リソースの設定

		// 描画コマンド
		commandList->DrawIndexedInstanced(kNumVertex, 1, 0, 0, 0);
	}

	void Sprite::Finalize()
	{
		// リソースの解放
		vertexResource.Reset();
		indexResource.Reset();
		materialResource.Reset();
		transformationMatrixResource.Reset();
		effectParamsResource.Reset();

		textureIndex_ = 0;

		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///						テクスチャの変更
	/// -------------------------------------------------------------
	void Sprite::SetTexture(const std::string& filePath)
	{
		// テクスチャを読み込む
		filePath_ = filePath;

		// テクスチャを読み込む
		textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);
	}

	void Sprite::SetReloadProgress(bool isReloading, float progress)
	{
		effectParamsData->isReloading = isReloading ? 1u : 0u;
		effectParamsData->reloadProgress = std::clamp(progress, 0.0f, 1.0f);
	}

	void Sprite::SetCrack(bool enable, float progress)
	{
		effectParamsData->enableCrack = enable ? 1u : 0u;
		effectParamsData->crackProgress = std::clamp(progress, 0.0f, 1.0f);
	}

	void Sprite::SetCrackParams(float scale, float thickness, float intensity, const Vector2& hitUV)
	{
		effectParamsData->crackScale = std::max(1.0f, scale);
		effectParamsData->crackThickness = std::max(0.0005f, thickness);
		effectParamsData->crackIntensity = std::max(0.0f, intensity);
		effectParamsData->crackHitUV = hitUV;
	}

	/// -------------------------------------------------------------
	///	 スプライト用のマテリアルリソースを作成し設定する処理を行う
	/// -------------------------------------------------------------
	void Sprite::CreateMaterialResource()
	{
		//スプライト用のマテリアルソースを作る
		materialResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));

		//書き込むためのアドレスを取得
		materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
		materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		materialData->textureIndex = textureIndex_; // テクスチャインデックスを設定
		//UVTransform行列を単位行列で初期化(スプライト用)
		materialData->uvTransform = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///	  スプライトの頂点バッファリソースと変換行列リソースを生成
	/// -------------------------------------------------------------
	void Sprite::CreateVertexBufferResource()
	{
		//Sprite用の頂点リソースを作る
		vertexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * kNumVertex);

		vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = sizeof(VertexData) * kNumVertex;
		vertexBufferView.StrideInBytes = sizeof(VertexData);

		vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

		//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
		transformationMatrixResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(TransformationMatrix));
		// 座標変換行列リソースにデータを書き込むためのアドレスを取得
		transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

		//単位行列を書き込んでおく
		transformationMatrixData->World = Matrix4x4::MakeIdentity();
		transformationMatrixData->WVP = Matrix4x4::MakeIdentity();
	}

	/// -------------------------------------------------------------
	///	   スプライトのインデックスバッファを作成および設定する
	/// -------------------------------------------------------------
	void Sprite::CreateIndexBuffer()
	{
		indexResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * kNumVertex);
		//リソースの先頭のアドレスから使う
		indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
		//使用するリソースのサイズはインデックス６つ分のサイズ
		indexBufferView.SizeInBytes = sizeof(uint32_t) * kNumVertex;
		//インデックスはuint32_tとする
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

		// インデックスデータにデータを書き
		indexData[0] = 0; // 三角形1つ目
		indexData[1] = 1; // 三角形1つ目
		indexData[2] = 2; // 三角形1つ目
		indexData[3] = 1; // 三角形2つ目
		indexData[4] = 3; // 三角形2つ目
		indexData[5] = 2; // 三角形2つ目
	}

	/// -------------------------------------------------------------
	///				　テクスチャサイズをイメージに合わせる
	/// -------------------------------------------------------------
	void Sprite::AdjustTextureSize()
	{
		// テクスチャメタデータを取得
		const DirectX::TexMetadata& metaData = TextureManager::GetInstance()->GetMetaData(filePath_);

		textureSize_.x = static_cast<float>(metaData.width);  // テクスチャ幅
		textureSize_.y = static_cast<float>(metaData.height); // テクスチャ高さ

		// 画像サイズをテクスチャサイズに合わせる
		size_ = textureSize_;
	}

	/// -------------------------------------------------------------
	///			　			リロード進捗の初期化処理
	/// -------------------------------------------------------------
	void Sprite::InitializeReloadProgress()
	{
		// エフェクトパラメータのリソースを作成
		effectParamsResource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(EffectParams));
		// エフェクトパラメータのリソースにデータを書き込むためのアドレスを取得
		effectParamsResource->Map(0, nullptr, reinterpret_cast<void**>(&effectParamsData));

		// エフェクトパラメータの初期化
		// 初期値
		effectParamsData->isReloading = false;
		effectParamsData->reloadProgress = 0.0f;

		effectParamsData->enableCrack = false;
		effectParamsData->crackProgress = 0.0f;

		effectParamsData->crackHitUV = { 0.5f, 0.5f };
		effectParamsData->crackScale = 8.0f;
		effectParamsData->crackThickness = 0.03f;
		effectParamsData->crackIntensity = 1.0f;
	}

} // namespace Ken4lowEngine
