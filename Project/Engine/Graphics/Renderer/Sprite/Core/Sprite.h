#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "Vector2.h"
#include "Vector4.h"

#include <algorithm>
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

		/// <summary>
		/// スプライトを初期化します。<br/>
		/// ・テクスチャの読み込みと SRV 取得<br/>
		/// ・インデックスバッファの生成<br/>
		/// ・マテリアル用定数バッファの生成と初期値設定<br/>
		/// ・頂点バッファ／座標変換行列用定数バッファの生成<br/>
		/// ・テクスチャサイズに合わせたサイズ調整<br/>
		/// ・リロード進捗用定数バッファの生成と初期化<br/>
		/// を行います。
		/// </summary>
		/// <param name="filePath">使用するテクスチャファイルのパス。</param>
		void Initialize(const std::string& filePath);

		/// <summary>
		/// スプライトの更新処理を行います。<br/>
		/// ・アンカーポイント／フリップ状態を反映した頂点座標の計算<br/>
		/// ・UV 範囲（leftTop, size）を反映したテクスチャ座標の計算<br/>
		/// ・正射影行列を用いた WVP 行列の再計算<br/>
		/// を行い、マッピング済みのバッファへ書き込みます。
		/// </summary>
		void Update();

		/// <summary>
		/// スプライトの描画処理を行います。<br/>
		/// ・VBV / IBV の設定<br/>
		/// ・マテリアル / リロード進捗 / 座標変換行列の CBV をルートに設定<br/>
		/// ・テクスチャ SRV をディスクリプタテーブルとして設定<br/>
		/// ・DrawIndexedInstanced による描画<br/>
		/// を行います。
		/// </summary>
		void Draw();

		/// <summary>
		/// スプライトの終了処理を行います。<br/>
		/// 内部で確保したリソースの解放を行います。
		/// </summary>
		void Finalize();

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// 左右反転フラグを取得します。
		/// </summary>
		/// <returns>左右フリップしている場合は true。</returns>
		bool GetFlipX() const { return isFlipX_; }

		/// <summary>
		/// 上下反転フラグを取得します。
		/// </summary>
		/// <returns>上下フリップしている場合は true。</returns>
		bool GetFlipY() const { return isFlipY_; }

		/// <summary>
		/// スプライトの左上座標（スクリーン座標）を取得します。
		/// </summary>
		/// <returns>座標 (x, y)。</returns>
		const Vector2& GetPosition() const { return position_; }

		/// <summary>
		/// スプライトの回転角を取得します（Z 軸周り・ラジアン）。
		/// </summary>
		/// <returns>回転角（ラジアン）。</returns>
		float GetRotation() const { return rotation_; }

		/// <summary>
		/// スプライトのサイズ（幅・高さ）を取得します。
		/// </summary>
		/// <returns>サイズ (width, height)。</returns>
		const Vector2& GetSize() const { return size_; }

		/// <summary>
		/// スプライトの色を取得します。
		/// </summary>
		/// <returns>RGBA 形式の色。</returns>
		const Vector4& GetColor() const { return materialData->color; }

		/// <summary>
		/// アンカーポイント（0〜1 の正規化座標）を取得します。<br/>
		/// (0,0)=左上, (0.5,0.5)=中心, (1,1)=右下。
		/// </summary>
		/// <returns>アンカーポイント (ax, ay)。</returns>
		const Vector2& GetAnchorPoint() const { return anchorPoint_; }

		/// <summary>
		/// テクスチャ内での切り出し領域の左上座標（ピクセル）を取得します。
		/// </summary>
		/// <returns>左上座標 (x, y)。</returns>
		const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }

		/// <summary>
		/// テクスチャ内での切り出しサイズ（ピクセル）を取得します。
		/// </summary>
		/// <returns>サイズ (width, height)。</returns>
		const Vector2& GetTextureSize() { return textureSize_; }
		// テクスチャ内での切り出しサイズ（ピクセル）を取得します。
		const Vector2& GetTextureSize() const { return textureSize_; }

		const uint32_t GetTextureIndex() const { return materialData->textureIndex; }

		// 使用しているテクスチャファイルのパスを取得します。
		const std::string& GetFilePath() const { return filePath_; }

	public: /// ---------- セッター ---------- ///

		/// <summary>
		/// 左右反転の状態を設定します。
		/// </summary>
		/// <param name="isFlipX">左右フリップする場合は true。</param>
		void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

		/// <summary>
		/// 上下反転の状態を設定します。
		/// </summary>
		/// <param name="isFlipY">上下フリップする場合は true。</param>
		void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

		/// <summary>
		/// スプライトの左上座標（スクリーン座標）を設定します。
		/// </summary>
		/// <param name="position">座標 (x, y)。</param>
		void SetPosition(const Vector2& position) { position_ = position; }

		/// <summary>
		/// スプライトの回転角を設定します（Z 軸周り・ラジアン）。
		/// </summary>
		/// <param name="rotation">回転角（ラジアン）。</param>
		void SetRotation(float rotation) { rotation_ = rotation; }

		/// <summary>
		/// スプライトのサイズ（幅・高さ）を設定します。
		/// </summary>
		/// <param name="size">サイズ (width, height)。</param>
		void SetSize(const Vector2& size) { size_ = size; }

		/// <summary>
		/// スプライトの色を設定します。
		/// </summary>
		/// <param name="color">RGBA 形式の色。</param>
		void SetColor(const Vector4& color) { materialData->color = color; }

		/// <summary>
		/// アンカーポイント（0〜1 の正規化座標）を設定します。<br/>
		/// 例: (0.5, 0.5) にするとスプライト中心回転になります。
		/// </summary>
		/// <param name="anchorPoint">アンカーポイント (ax, ay)。</param>
		void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

		/// <summary>
		/// テクスチャ内での切り出し領域の左上座標（ピクセル）を設定します。
		/// </summary>
		/// <param name="textureLeftTop">左上座標 (x, y)。</param>
		void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

		/// <summary>
		/// テクスチャ内での切り出しサイズ（ピクセル）を設定します。
		/// </summary>
		/// <param name="textureSize">サイズ (width, height)。</param>
		void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

		/// <summary>
		/// 使用するテクスチャを変更します。<br/>
		/// Initialize 済みの Sprite に対して、別の SRV を割り当てたい場合に使います。
		/// </summary>
		/// <param name="filePath">新しく使用するテクスチャファイルのパス。</param>
		void SetTexture(const std::string& filePath);

		/// <summary>
		/// テクスチャ内の UV 切り出し範囲をまとめて設定します。<br/>
		/// leftTop / size から textureLeftTop_ と textureSize_ を更新します。
		/// </summary>
		/// <param name="leftTop">切り出し左上座標（ピクセル）。</param>
		/// <param name="size">切り出しサイズ（ピクセル）。</param>
		void SetUVRect(const Vector2& leftTop, const Vector2& size) { textureLeftTop_ = leftTop; textureSize_ = size; }

		/// <summary>
		/// リロード進捗を設定します。<br/>
		/// ・isReloading : リロード中かどうか<br/>
		/// ・progress   : 進捗度合い (0.0〜1.0)<br/>
		/// この値は ReloadProgress 用の定数バッファに書き込まれ、ピクセルシェーダで利用されます。
		/// </summary>
		/// <param name="isReloading">リロード中かどうか。</param>
		/// <param name="reloadProgress">進捗度合い (0.0〜1.0)。</param>
		void SetReloadProgress(bool isReloading, float progress);

		void SetCrack(bool enable, float progress);

		void SetCrackParams(float scale, float thickness, float intensity, const Vector2& hitUV);

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// スプライト用のマテリアルリソースを作成し、初期値を設定します。<br/>
		/// ・色を白 (1,1,1,1)<br/>
		/// ・UVTransform を単位行列<br/>
		/// で初期化します。
		/// </summary>
		void CreateMaterialResource();

		/// <summary>
		/// スプライトの頂点バッファと、座標変換行列用の定数バッファを生成します。<br/>
		/// ・VertexData × kNumVertex 分の頂点バッファを作成し、マッピングしてポインタを保持<br/>
		/// ・TransformationMatrix 用の定数バッファを作成し、単位行列で初期化<br/>
		/// を行います。
		/// </summary>
		void CreateVertexBufferResource();

		/// <summary>
		/// スプライト用のインデックスバッファを生成し、0〜5 のインデックスを設定します。<br/>
		/// 2 枚の三角形で長方形を構成するためのインデックスになります。
		/// </summary>
		void CreateIndexBuffer();

		/// <summary>
		/// テクスチャのメタデータを参照し、textureSize_ と size_ をテクスチャの幅・高さに合わせます。
		/// </summary>
		void AdjustTextureSize();

		/// <summary>
		/// リロード進捗用定数バッファの初期化を行います。<br/>
		/// ・ReloadProgress 用バッファを生成<br/>
		/// ・マッピングして reloadProgressData ポインタを取得<br/>
		/// ・初期値（isReloading, progress）を書き込み<br/>
		/// を行います。
		/// </summary>
		void InitializeReloadProgress();

	private: /// ---------- メンバ変数 ---------- ///

		// 左右フリップ
		bool isFlipX_ = false;

		// 上下フリップ
		bool isFlipY_ = false;

		// 座標
		Vector2 position_ = { 0.0f, 0.0f };
		// 回転
		float rotation_ = 0;
		// サイズ
		Vector2 size_ = { 1.0f, 1.0f };
		//アンカーポイント
		Vector2 anchorPoint_ = { 0.0f, 0.0f };
		// テクスチャ左上座標
		Vector2 textureLeftTop_ = { 0.0f, 0.0f };
		// テクスチャ切り出しサイズ
		Vector2 textureSize_ = { 100.0f, 100.0f };
		// 色
		Vector4 color_ = {};

		// テクスチャ番号
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_{};

		// テクスチャファイルのパス
		std::string filePath_;

		// テクスチャインデックス
		uint32_t textureIndex_ = 0;

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		//スプライト用のマテリアルソースを作る
		ComPtr <ID3D12Resource> materialResource;
		Material* materialData = nullptr;

		// スプライトの頂点バッファリソースと変換行列リソースを生成
		//Sprite用の頂点リソースを作る
		ComPtr <ID3D12Resource> vertexResource;
		//頂点バッファビューを作成する
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		// 頂点データを設定する
		VertexData* vertexData = nullptr;
		//Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
		ComPtr <ID3D12Resource> transformationMatrixResource;
		//データを書き込む
		TransformationMatrix* transformationMatrixData = nullptr;

		// スプライトのインデックスバッファを作成および設定する
		ComPtr <ID3D12Resource> indexResource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView{};
		uint32_t* indexData = nullptr;

		// リロード進捗のリソース
		ComPtr <ID3D12Resource> effectParamsResource;
		EffectParams* effectParamsData = nullptr;
	};


} // namespace Ken4lowEngine
