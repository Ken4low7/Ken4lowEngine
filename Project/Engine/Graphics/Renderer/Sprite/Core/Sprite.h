#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "Vector2.h"
#include "Vector4.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;


	/// -------------------------------------------------------------
	///						スプライトクラス
	/// -------------------------------------------------------------
	class Sprite
	{
	private: /// ---------- 定数 ---------- ///

		static inline const UINT kNumVertex = 6; // 四角形を描画するための頂点数
		static inline const UINT kNumIndex = 4;  // 四角形を描画するためのインデックス数

	private: /// ---------- 構造体　----------- ///

		// マテリアルデータの構造体
		struct Material final
		{
			Vector4 color;		   // 色(RGBA)
			Matrix4x4 uvTransform; // UV変換行列
			uint32_t textureIndex; // テクスチャインデックス
			float padding[3];	   // パディング
		};

		// 頂点データの構造体
		struct VertexData
		{
			Vector4 position; // 座標
			Vector2 texcoord; // テクスチャ座標
		};

		// 座標変換行列データの構造体
		struct TransformationMatrix final
		{
			Matrix4x4 WVP;	 // ワールドビュー射影変換行列
			Matrix4x4 World; // ワールド変換行列
		};

	private: /// ---------- ゲーム用の構造体 ---------- ///

		// リロード進捗の構造体
		struct EffectParams
		{
			bool isReloading = false; // リロード中かどうか
			float reloadProgress = 0.0f;	  // 進捗度合い(0.0f〜1.0f)

			bool enableCrack = false; // ひび割れエフェクトを有効にするかどうか
			float crackProgress = 0.0f;  // ひび割れ進捗度合い(0.0f〜1.0f)

			Vector2 crackHitUV = { 0.5f, 0.5f }; // ひび割れの中心UV座標
			float crackScale = 18.0f;		   // ひび割れの大きさスケール
			float crackThickness = 0.03f;	   // ひび割れの太さ

			uint32_t crackSpeed = 1;		   // ひび割れの進行速度
			float crackIntensity = 1.0f;	   // ひび割れの強さ

			float padding[2];		  // パディング
		};

	public: /// ---------- メンバ関数 ---------- ///

		void Initialize(const std::string& filePath);
		void Update();
		void Draw();
		void Finalize();

	public: /// ---------- ゲッター ---------- ///

		bool GetFlipX() const { return isFlipX_; }
		bool GetFlipY() const { return isFlipY_; }
		const Vector2& GetPosition() const { return position_; }
		float GetRotation() const { return rotation_; }
		const Vector2& GetSize() const { return size_; }
		const Vector4& GetColor() const { return materialData_.color; }
		const Vector2& GetAnchorPoint() const { return anchorPoint_; }
		const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
		const Vector2& GetTextureSize() { return textureSize_; }
		const Vector2& GetTextureSize() const { return textureSize_; }
		const uint32_t GetTextureIndex() const { return materialData_.textureIndex; }
		const std::string& GetFilePath() const { return filePath_; }

	public: /// ---------- セッター ---------- ///

		void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
		void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
		void SetPosition(const Vector2& position) { position_ = position; }
		void SetRotation(float rotation) { rotation_ = rotation; }
		void SetSize(const Vector2& size) { size_ = size; }
		void SetColor(const Vector4& color) { materialData_.color = color; }
		void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
		void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }
		void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }
		void SetTexture(const std::string& filePath);
		void SetUVRect(const Vector2& leftTop, const Vector2& size) { textureLeftTop_ = leftTop; textureSize_ = size; }
		void SetReloadProgress(bool isReloading, float progress);
		void SetCrack(bool enable, float progress);
		void SetCrackParams(float scale, float thickness, float intensity, const Vector2& hitUV);

	private: /// ---------- メンバ関数 ---------- ///

		void CreateMaterialResource();
		void CreateVertexBufferResource();
		void CreateIndexBuffer();
		void AdjustTextureSize();
		void InitializeReloadProgress();

	private: /// ---------- メンバ変数 ---------- ///

		bool isFlipX_ = false;
		bool isFlipY_ = false;
		Vector2 position_ = { 0.0f, 0.0f };
		float rotation_ = 0;
		Vector2 size_ = { 1.0f, 1.0f };
		Vector2 anchorPoint_ = { 0.0f, 0.0f };
		Vector2 textureLeftTop_ = { 0.0f, 0.0f };
		Vector2 textureSize_ = { 100.0f, 100.0f };
		Vector4 color_ = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};
		std::string filePath_;
		uint32_t textureIndex_ = 0;

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		Material materialData_{};
		std::array<VertexData, kNumVertex> vertexData_{};
		TransformationMatrix transformationMatrixData_{};
		EffectParams effectParamsData_{}; // 描画時にFrame Upload Arenaへコピーし、前FrameのGPU参照とCPU更新を分離する。

		ComPtr<ID3D12Resource> indexResource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		uint32_t* indexData = nullptr;
	};


} // namespace Ken4lowEngine
