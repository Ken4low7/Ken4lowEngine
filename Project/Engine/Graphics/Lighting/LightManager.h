#pragma once
#include "DX12Include.h"
#include "Vector3.h" 
#include "Vector4.h"
#include "Matrix4x4.h"
#include "LightParameterController.h"
#include "ShadowSettings.h"
#include <memory>
#include <string>
#include <functional>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class LightGpuBuffer;
	class LightEditorPanel;
	class LightPresetService;
	class ShadowSystem;


	/// -------------------------------------------------------------
	///				　		ライトの管理クラス
	/// -------------------------------------------------------------
	class LightManager
	{
	private: /// ---------- コンストラクタ・デストラクタ ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンパターンとして利用します。
		/// </summary>
		LightManager() = default;

		/// <summary>
		/// デフォルトデストラクタ。
		/// </summary>
		~LightManager();

	public: /// ---------- 構造体 ---------- ///

		// パンクチュアルライトの構造体
		struct PunctualLightGPU
		{
			uint32_t lightType;		// ライトの種類（0：None、1：Directional、2：Point、3：Spot、4：RectArea、5：SphereArea）
			Vector4 color;			// ライトの色 （全ライト共通）
			float intensity;		// 輝度 （全ライト共通）
			Vector3 position;		// ライトの位置 （点光源、スポットライト用）
			float radius;			// ライトの届く最大距離 （点光源用）
			float decay;			// 減衰率 （点光源、スポットライト用）
			Vector3 direction;		// スポットライトの方向 （平行光源、スポットライト用）
			float distance;			// ライトの届く最大距離 （スポットライト用）
			float cosFalloffStart;	// 開始角度 （スポットライト用）
			float cosAngle;			// スポットライトの余弦 （スポットライト用）
			Vector3 areaSize;		// 疑似AreaLightサイズ (x=width, y=height, z=unused/radius)
			uint32_t enabled;		// 0: disabled, 1: enabled
		};


		// シャドウ行列の生成対象を明示するための種別。
		enum class ShadowCasterType : uint32_t
		{
			None = 0,
			Directional = 1,
			Spot = 2,
			Point = 3,
		};

		// 既存コードの LightManager::ShadowFocusMode 参照を維持するための互換エイリアス。
		// 実体はShadowSettings.h側へ移し、Shadow設定だけを将来のShadowSystemへ渡しやすくする。
		using ShadowFocusMode = ::Ken4lowEngine::ShadowFocusMode;
		using ShadowSettings = ::Ken4lowEngine::ShadowSettings;
		// ライト数CB
		struct LightInfo
		{
			uint32_t lightCount; // ライトの数
			float pad[3]; 		 // パディング
		};

		// ステージを白飛びさせないためのライティング/露出調整CB。
		struct LightingSettingsGPU
		{
			Vector4 ambientColor = { 0.10f, 0.10f, 0.10f, 0.15f };
			Vector4 fogColor = { 0.58f, 0.64f, 0.70f, 1.0f };
			float exposure = 1.0f;
			float contrast = 1.0f;
			float fogStart = 45.0f;
			float fogEnd = 140.0f;
			uint32_t enableFog = 0;
			float specularStrength = 0.08f;
			float diffuseStrength = 1.0f;
			float specularPowerScale = 1.0f;
			float rimLightStrength = 0.0f;
			float rimLightPower = 2.0f;
			uint32_t enableRimLight = 0;
			uint32_t enableHalfLambert = 0;
			Vector4 rimLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			uint32_t shadingMode = 0;
			uint32_t enableIBL = 0; // PBR用IBL入口。初期OFFで既存Legacyの見た目を維持する。
			float iblDiffuseStrength = 0.0f; // 環境マップ未設定時の安全な拡散環境光fallback強度。
			float iblSpecularStrength = 0.0f; // prefilteredEnv未実装時の簡易反射fallback強度。
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// LightManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>LightManager の唯一のインスタンス。</returns>
		static LightManager* GetInstance();

		/// <summary>
		/// ライトマネージャの初期化処理。<br/>
		/// ・DirectXCommon の保持<br/>
		/// ・ライト数用 CBV の作成とマップ<br/>
		/// ・パンクチュアルライト用 Structured Buffer / SRV の準備（必要最小限のサイズ）<br/>
		/// を行います。
		/// </summary>
		/// <param name="dxCommon">DirectX12 共通クラスへのポインタ。</param>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// ライトマネージャの終了処理。<br/>
		/// 内部で確保したリソースを解放します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// ImGui のライト編集UIを描画します。<br/>
		/// 既存public API互換の入口として残し、実際のUI構築はLightEditorPanelへ委譲します。
		/// </summary>
		void DrawImGui(bool* pOpen = nullptr);

		/// <summary>
		/// Details Inspectorと専用Light Editorで同じGlobal Lighting/Debug UIを共有するための互換入口です。<br/>
		/// Runtime所有者のLightManagerからUI責務を分けるため、中身はLightEditorPanelへ委譲します。
		/// </summary>
		void DrawPunctualLightsInspector();

		/// <summary>
		/// パンクチュアルライト情報をシェーダにバインドします。<br/>
		/// 内部で UpdatePunctualLight() を呼び出して GPU バッファと SRV を更新し、<br/>
		/// ・b2: LightInfo 用 CBV<br/>
		/// ・t2: パンクチュアルライト配列用 SRV<br/>
		/// をそれぞれルートパラメータに設定します。
		/// </summary>
		/// <param name="rootIndexCB_b2">ライト数 CBV をバインドするルートインデックス（b2）。</param>
		/// <param name="rootIndexSRV_t2">パンクチュアルライト SRV をバインドするルートインデックス（t2）。</param>
		void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);

		/// <summary>ライティング調整CBをシェーダにバインドします。</summary>
		void BindLightingSettings(uint32_t rootIndexCB_b5);

		/// <summary>
		/// デフォルトの指向性ライトを追加します。
		/// </summary>
		void AddDefaultDirectionalLight();

		/// <summary>既定プリセットを優先して初期ライトを再構成します。</summary>
		void ResetToDefaultLighting();

		/// <summary>
		/// LightPresetを保存する既存public API互換入口です。<br/>
		/// 保存形式と呼び出し側を変えないため関数名は残し、実体処理はLightPresetServiceへ委譲します。
		/// </summary>
		bool SaveLightPreset(const std::string& assetId);

		/// <summary>
		/// LightPresetを指定パスから読み込む既存public API互換入口です。<br/>
		/// LightManagerはFacadeとして残り、JSONの読み書き責務はLightPresetServiceへ分離します。
		/// </summary>
		bool ApplyLightPresetByPath(const std::string& filePath);

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// 点光源のリストを取得します。
		/// </summary>
		/// <returns>PunctualLightGPU オブジェクトのベクトルへの const 参照。</returns>
		const std::vector<PunctualLightGPU>& GetPunctualLights() const { return punctualLights_; }
		const LightingSettingsGPU& GetLightingSettings() const { return lightingSettings_; }
		float GetShadowBias() const { return shadowBias_; }
		float GetNormalBias() const { return normalBias_; }
		float GetShadowStrength() const { return shadowStrength_; }
		bool IsShadowEnabled() const { return enableShadow_; }
		uint32_t GetShadowMapSize() const { return shadowMapSize_; }
		bool IsShadowMapDebugEnabled() const { return showShadowMapDebug_; }
		bool IsShadowFactorDebugEnabled() const { return showShadowFactorDebug_; }
		LightingSettingsGPU& GetMutableLightingSettingsForEditor() { return lightingSettings_; }
		ShadowCasterType GetActiveShadowCasterType() const;
		uint32_t GetShadowReceiverMode() const;
		Matrix4x4 BuildShadowLightViewProjection(const Vector3& focusPosition) const;
		bool TryGetActiveShadowCasterLightInfo(int32_t& outIndex, PunctualLightGPU& outLight, ShadowCasterType& outType) const;
		void SetShadowCasterLightIndex(int32_t index) { shadowCasterLightIndex_ = index; }
		int32_t GetShadowCasterLightIndex() const { return shadowCasterLightIndex_; }
		void SetShadowFocusMode(ShadowFocusMode mode) { shadowFocusMode_ = mode; }
		void SetManualShadowFocusPosition(const Vector3& pos) { manualShadowFocusPosition_ = pos; }
		void SetDirectionalShadowFrustum(float width, float height, float nearZ, float farZ) { directionalShadowWidth_ = width; directionalShadowHeight_ = height; directionalShadowNearZ_ = nearZ; directionalShadowFarZ_ = farZ; }
		void SetShadowMapSize(uint32_t size) { shadowMapSize_ = size; }

		// Editor Detailsから選択中ライトだけを書き換えるため、index検証付きヘルパー経由でのみ利用する。
		std::vector<PunctualLightGPU>& GetMutablePunctualLightsForEditor() { return punctualLights_; }

		/// <summary>
		/// Actorに追加されたLightComponent由来のライト一覧を反映します。<br/>
		/// 個別ライトの編集元はActor DetailsのLightComponentで、LightManager側ではGPU転送とDebug表示だけを行います。
		/// </summary>
		void SetLightComponentLights(const std::vector<PunctualLightGPU>& lights);

		/// <summary>
		/// 旧PointLight時代の互換入口です。<br/>
		/// 現在はDirectional/Point/Spot/Areaを含むLightComponentライト全体をSetLightComponentLightsへ転送します。
		/// </summary>
		void SetLightComponentPointLights(const std::vector<PunctualLightGPU>& lights);

		/// <summary>Light Editorの読み取り専用Debug表示で使用するLightComponent由来ライト一覧を返します。</summary>
		const std::vector<PunctualLightGPU>& GetLightComponentLightsForDebug() const { return lightComponentLights_; }

	public: /// ---------- ParameterManager連携用の窓口 ---------- ///

		/// <summary>
		/// ParameterManager登録時に現在のLighting設定を読み取るための参照を返します。<br/>
		/// LightParameterController専用の窓口で、個別メンバを公開しすぎないためカテゴリ単位で受け渡します。
		/// </summary>
		const LightingSettingsGPU& GetLightingSettingsForParameter() const { return lightingSettings_; }

		/// <summary>
		/// ParameterManagerから読み込んだLighting設定を反映するための編集用参照を返します。<br/>
		/// LightManager本体はGPU転送に集中し、保存値の検証はLightParameterController側で行います。
		/// </summary>
		LightingSettingsGPU& GetMutableLightingSettingsForParameter() { return lightingSettings_; }

		/// <summary>
		/// ParameterManager登録・同期用にパンクチュアルライト配列を読み取ります。<br/>
		/// 現状の保存対象はLight #0のみですが、配列全体の所有はLightManagerに残します。
		/// </summary>
		const std::vector<PunctualLightGPU>& GetPunctualLightsForParameter() const { return punctualLights_; }

		/// <summary>
		/// ParameterManagerからLight #0を反映するための編集用ライト配列を返します。<br/>
		/// LightParameterControllerは必要な最小範囲だけを更新し、追加/削除UIの責務はLightManager側に残します。
		/// </summary>
		std::vector<PunctualLightGPU>& GetMutablePunctualLightsForParameter() { return punctualLights_; }

		/// <summary>
		/// Light #0保存対象が空配列で欠けている場合に、既定のDirectionalLightを補います。<br/>
		/// 保存済みJSONの反映先を必ず用意し、空配列アクセスによるクラッシュを防ぎます。
		/// </summary>
		void EnsureDefaultLightForParameter();

		/// <summary>
		/// ParameterManagerへ登録・同期するShadow設定をカテゴリ単位で取得します。<br/>
		/// privateメンバを直接公開せず、LightParameterControllerとの境界を狭く保つための窓口です。
		/// </summary>
		ShadowSettings GetShadowSettingsForParameter() const;

		/// <summary>
		/// ParameterManagerから検証済みのShadow設定をLightManagerへまとめて反映します。<br/>
		/// shadowMapSizeはGPUリソース再生成が必要なため、専用関数経由で適用します。
		/// </summary>
		void SetShadowSettingsFromParameter(const ShadowSettings& settings);

		/// <summary>
		/// ParameterManager由来のshadowMapSizeをLightManagerへ反映します。<br/>
		/// サイズ変更時だけShadowMapリソースを再生成し、毎回の無駄なGPUリソース更新を避けます。
		/// </summary>
		void ApplyShadowMapSizeFromParameter(uint32_t size);

		/// <summary>選択ライトに応じたLegacy/Spot、Point 6面、CSM 4層のShadow Passを実行します。</summary>
		void ExecuteShadowPasses(const std::function<void()>& drawShadowObjects);

		/// <summary>既存b4を維持したまま追加したb6/t10/t11を通常描画へバインドします。</summary>
		void BindExtendedShadowResources(uint32_t extendedShadowCbvRootIndex, uint32_t csmSrvRootIndex, uint32_t pointSrvRootIndex) const;

		/// <summary>現在描画中のShadow Sliceで使用するLight ViewProjectionを返します。</summary>
		const Matrix4x4& GetActiveShadowPassLightViewProjection() const;

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// パンクチュアルライト用GPUリソース生成の互換的な内部入口です。<br/>
		/// LightManager本体からGPUリソース管理を分けるため、実体処理はLightGpuBufferへ委譲します。
		/// </summary>
		void CreatePunctualLight();

		/// <summary>
		/// パンクチュアルライトのGPU転送を行う互換的な内部入口です。<br/>
		/// 転送実体はLightGpuBufferへ委譲し、LightManager側にはデバッグギズモ描画だけを残します。
		/// </summary>
		void UpdatePunctualLight();

		/// <summary>
		/// デバッグ用のライトギズモを描画します。<br/>
		/// Wireframe クラスを用いて、ライトの種類に応じて：<br/>
		/// ・平行光源：原点付近に矢印線<br/>
		/// ・点光源：位置に小さな球＋到達半径の球（半透明）<br/>
		/// ・スポットライト：位置に球＋方向線<br/>
		/// を描画します。
		/// </summary>
		void DebugDrawLightGizmos();

	private: /// ---------- メンバ変数 ---------- ///

		friend class LightEditorPanel; // UI専用クラスだけが既存Inspector表示に必要な内部状態を読み書きする。
		friend class LightPresetService; // Preset保存/読み込み専用クラスだけが既存JSON互換のため状態を写し取る。
		friend class ShadowSystem; // 複数Shadow Passの行列生成とGPU転送だけがShadow設定/ライト配列を参照する。

		DirectXCommon* dxCommon_ = nullptr;
		std::unique_ptr<LightGpuBuffer> lightGpuBuffer_;
		std::unique_ptr<ShadowSystem> shadowSystem_;

		std::vector<PunctualLightGPU> punctualLights_; // GPUに送るグローバル/Legacyライト情報
		std::vector<PunctualLightGPU> lightComponentLights_; // LightComponentから収集した全ライト情報

		uint32_t punctualType_ = 1; // 0=None, 1=Directional, 2=Point, 3=Spot
		LightingSettingsGPU lightingSettings_{};
		bool enableShadow_ = true;
		float shadowBias_ = 0.0f;
		float normalBias_ = 0.025f;
		float shadowStrength_ = 0.6f;
		uint32_t shadowMapSize_ = 2048;
		bool showShadowMapDebug_ = false;
		bool showShadowFactorDebug_ = false;

		// ステージに合わせてDirectionalLightの影有効範囲を調整するための設定値。
		float directionalShadowDistance_ = 60.0f;
		float directionalShadowWidth_ = 35.0f;
		float directionalShadowHeight_ = 35.0f;
		float directionalShadowNearZ_ = 0.1f;
		float directionalShadowFarZ_ = 120.0f;
		float directionalShadowFocusOffset_ = 0.0f;
		ShadowFocusMode shadowFocusMode_ = ShadowFocusMode::Camera;
		Vector3 manualShadowFocusPosition_ = { 0.0f, 0.0f, 0.0f };
		mutable Vector3 currentShadowFocusPosition_ = { 0.0f, 0.0f, 0.0f };
		mutable Vector3 currentShadowDirection_ = { 0.0f, -1.0f, 0.0f };
		mutable Matrix4x4 currentShadowLightViewProjection_ = Matrix4x4::MakeIdentity();
		mutable float currentShadowFrustumWidth_ = 35.0f;
		mutable float currentShadowFrustumHeight_ = 35.0f;
		mutable float currentShadowFrustumNearZ_ = 0.1f;
		mutable float currentShadowFrustumFarZ_ = 120.0f;
		float spotShadowNearZ_ = 0.1f;
		float pointShadowNearZ_ = 0.1f;
		bool enableCsm_ = false;
		float csmMaxDistance_ = 160.0f;
		float csmSplitLambda_ = 0.7f;
		int32_t shadowCasterLightIndex_ = -1;
		LightParameterController lightParameterController_;

	private: /// ---------- コピー禁止 ---------- ///

		/// <summary>
		/// コピーコンストラクタは禁止。
		/// </summary>
		LightManager(const LightManager&) = delete;

		/// <summary>
		/// コピー代入演算子は禁止。
		/// </summary>
		LightManager& operator=(const LightManager&) = delete;

		/// <summary>
		/// ムーブコンストラクタは禁止。
		/// </summary>
		LightManager(LightManager&&) = delete;

		/// <summary>
		/// ムーブ代入演算子は禁止。
		/// </summary>
		LightManager& operator=(LightManager&&) = delete;
	};


} // namespace Ken4lowEngine
