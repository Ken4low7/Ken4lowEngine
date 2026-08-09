#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "Camera.h"
#include "CloudLayer.h"

#include "Vector2.h"
#include "Vector4.h"

#include <array>
#include <memory>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;

	/// -------------------------------------------------------------
	///                        スカイボックスクラス
	/// -------------------------------------------------------------
	/// 役割:
	/// - 環境テクスチャを使ったスカイボックスの頂点/インデックスデータを保持する
	/// - マテリアル / WVP 定数バッファを更新する
	/// - SkyBoxManager が設定した描画状態のもとでキューブを描画する
	///
	/// 注意:
	/// - RootSignature / PSO の生成は行わない
	/// - 描画前の共通レンダリング設定は SkyBoxManager 側が担当する
	/// -------------------------------------------------------------
	class SkyBox
	{
	private: /// ---------- 構造体 ---------- ///

		/// キューブの頂点数 / インデックス数
		static inline const UINT kNumVertex = 24;
		static inline const UINT kNumIndex = 36;

		/// <summary>
		/// PixelShader 側へ渡すマテリアルデータ。
		/// </summary>
		struct Material final
		{
			Vector4 color;
			Matrix4x4 uvTransform;
			Vector4 topColor;
			Vector4 bottomColor;
			Vector4 horizonColor;
			uint32_t textureIndex;
			uint32_t skyType;
			Vector2 uvOffset;
			float cloudHeight;
			float cloudScale;
			uint32_t textureAvailable;
			float padding;
		};

		/// <summary>
		/// SkyBox 頂点データ。
		/// position は clip 変換前のローカル座標、
		/// texcoord は環境テクスチャ参照方向として使う。
		/// </summary>
		struct VertexData
		{
			Vector4 position;
			Vector3 texcoord;
		};

		/// <summary>
		/// VertexShader 側へ渡す変換行列データ。
		/// </summary>
		struct TransformationMatrix final
		{
			Matrix4x4 WVP;
			Matrix4x4 World;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// スカイボックス描画を初期化する。
		/// </summary>
		/// <param name="filePath">
		/// 環境テクスチャのファイルパス。
		/// </param>
		///
		/// 実行内容:
		/// - DirectXCommon と使用カメラを取得する
		/// - 環境テクスチャを読み込む
		/// - スカイボックス用のスケール・回転・位置を初期化する
		/// - マテリアル / 頂点 / インデックス / WVP バッファを生成する
		void Initialize(const std::string& filePath);

		/// <summary>
		/// 毎フレームの更新処理を行う。
		/// </summary>
		///
		/// 実行内容:
		/// - ワールド行列を計算する
		/// - 通常カメラまたはデバッグカメラの ViewProjection を選ぶ
		/// - World * ViewProjection を計算して WVP バッファへ反映する
		void Update();

		/// <summary>
		/// スカイボックスを描画する。
		/// </summary>
		///
		/// 前提:
		/// - SkyBoxManager 側で RootSignature / PSO / PrimitiveTopology が設定済み
		///
		/// 実行内容:
		/// - 頂点 / インデックスバッファをバインドする
		/// - Material / WVP 定数バッファをバインドする
		/// - DrawIndexedInstanced でキューブを描画する
		void Draw();

		/// SkyBox の直後に半透明の雲レイヤーを描画する。
		void DrawCloudLayer();

		/// <summary>
		/// デバッグカメラを使用するかを設定する。
		/// </summary>
		void SetDebugCamera(bool isDebugCamera) { isDebugCamera_ = isDebugCamera; }

		/// <summary>
		/// デバッグカメラ使用状態を取得する。
		/// </summary>
		bool GetDebugCamera() { return isDebugCamera_; }

		/// <summary>
		/// 環境マップの GPU ハンドルを取得する。
		/// </summary>
		/// <remarks>
		/// IBL など別用途で環境マップを参照したい場合を想定。
		/// </remarks>
		D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMapHandle() const { return gpuHandle_; }

	public: /// ---------- アクセッサ ---------- ///

		/// ワールド変換の取得 / 設定
		WorldTransform& GetWorldTransform() { return worldTransform_; }
		void SetWorldTransform(const WorldTransform& worldTransform) { worldTransform_ = worldTransform; }

		/// 環境テクスチャを切り替え、必要に応じてディスクから再読み込みする。
		void SetTexture(const std::string& filePath, bool reloadTexture = false);
		const std::string& GetTexturePath() const { return texturePath_; }

		/// 色味と明るさを合成した描画色を設定する。
		void SetColor(const Vector4& color);
		void SetSkyType(const std::string& skyType);
		void SetGradientColors(const Vector4& topColor, const Vector4& bottomColor, const Vector4& horizonColor);
		void SetCloudLayer(bool enabled, const std::string& texturePath, float height, float scale,
			const Vector2& scrollSpeed, const Vector2& uvOffset, float alpha, const Vector4& tintColor, bool reloadTexture = false);
		void AdvanceCloudLayer(float deltaTime);
		Vector2 GetCloudUvOffset() const { return cloudLayer_->GetUvOffset(); }
		bool IsCloudTextureAvailable() const { return cloudLayer_->IsTextureAvailable(); }
		bool IsCloudEnabled() const { return cloudLayer_->IsEnabled(); }
		const std::string& GetCloudTexturePath() const { return cloudLayer_->GetTexturePath(); }
		uint32_t GetCloudTextureIndex() const { return cloudLayer_->GetTextureIndex(); }

	private: /// ---------- 内部メンバ関数 ---------- ///

		/// <summary>
		/// マテリアル用定数バッファを生成・初期化する。
		/// </summary>
		void InitializeMaterial();

		/// <summary>
		/// キューブ形状の頂点バッファを生成・初期化する。
		/// </summary>
		void InitializeVertexBufferData();

		/// <summary>
		/// キューブ形状のインデックスバッファを生成・初期化する。
		/// </summary>
		void InitializeIndexData();

	private: /// ---------- メンバ変数 ---------- ///

		/// D3D12 リソース生成やコマンド取得に使用する共通クラス
		DirectXCommon* dxCommon_ = nullptr;

		/// 描画に使用するカメラ
		Camera* camera_ = nullptr;

		/// SkyBox 自身のワールド変換
		WorldTransform worldTransform_;

		/// 環境マップ用 GPU ハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_ = {};

		/// Material 定数バッファ
		ComPtr<ID3D12Resource> materialResource;
		Material* materialData_ = nullptr;

		/// 頂点バッファ
		ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		VertexData* vertexData_ = nullptr;

		/// WVP 定数バッファ
		ComPtr<ID3D12Resource> wvpResource;
		TransformationMatrix* wvpData = nullptr;

		/// インデックスバッファ
		ComPtr<ID3D12Resource> indexResource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		uint32_t* indexData_ = nullptr;

		/// 計算済み行列
		Matrix4x4 worldViewProjectionMatrix;
		Matrix4x4 viewProjectionMatrix_;
		Matrix4x4 debugViewProjectionMatrix_;

		/// デバッグカメラを使うかどうか
		bool isDebugCamera_ = false;

		/// 使用中の環境テクスチャ index
		uint32_t textureIndex_ = 0;

		/// 使用中の環境テクスチャパス
		std::string texturePath_;
		uint32_t skyType_ = 2;
		Vector4 baseColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 topColor_ = { 0.20f, 0.55f, 0.95f, 1.0f };
		Vector4 bottomColor_ = { 0.72f, 0.88f, 1.0f, 1.0f };
		Vector4 horizonColor_ = { 0.92f, 0.96f, 1.0f, 1.0f };
		bool textureAvailable_ = false;
		std::unique_ptr<CloudLayer> cloudLayer_;
	};

} // namespace Ken4lowEngine
