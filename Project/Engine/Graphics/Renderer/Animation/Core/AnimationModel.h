#pragma once
#include "DX12Include.h"
#include "Vector3.h"
#include "Quaternion.h"
#include "ModelData.h"
#include "WorldTransform.h"
#include "Material.h"
#include "AnimationMesh.h"
#include "Skeleton.h"
#include "SkeletonAnimator.h"
#include <SkinCluster.h>
#include <Sphere.h>
#include "Capsule.h"
#include "TransformationMatrix.h"

#include "AnimationLoader.h"
#include "AnimationPlayer.h"
#include "AnimationModelLODBuilder.h"
#include "AnimationModelSkinningCS.h"
#include "AnimationModelLODController.h"
#include "AnimationModelColliderController.h"

#include <string>
#include <vector>
#include <memory>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class Camera;

	/// -------------------------------------------------------------
	///　　　　　　　アニメーションを描画するクラス
	/// -------------------------------------------------------------
	class AnimationModel
	{
	private: /// ---------- 構造体 ---------- ///

		// シェーダー側のカメラ構造体
		struct CameraForGPU
		{
			Vector3 worldPosition; // ワールド座標系でのカメラ位置
		};

		struct ShadowParameterForGPU
		{
			Matrix4x4 lightViewProjection; // ライトのビュー射影行列
			float shadowBias;              // シャドウバイアス
			float normalBias;              // 法線方向オフセット量
			float shadowStrength;          // 影の濃さ（DirectLight のみへ適用）
			uint32_t shadowMode;           // 0:Off 1:Directional 2:Spot 3:PointCube 4:CSM
			uint32_t shadowDebugMode;      // 0:None 1:ShadowMap 2:ShadowFactor
			float padding[1];              // パディング
		};

		// 互換用：旧 AnimationModel::BodyPartCollider 名を維持
		using BodyPartCollider = ::Ken4lowEngine::BodyPartCollider;

		// ボディパートコライダーは AnimationModelColliderController に集約
		AnimationModelColliderController colliderController_;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 指定したモデルファイルを読み込み、アニメーションメッシュやスケルトン、
		/// スキンクラスタなどのリソースを初期化します。
		/// </summary>
		/// <param name="fileName">読み込むモデルファイル名（相対パス）。</param>
		/// <param name="isSkinning">true の場合はスキニング用リソースも初期化します。</param>
		void Initialize(const std::string& fileName, bool isSkinning = true);

		/// <summary>
		/// モデル本体とアニメーションを別ファイルから読み込んで初期化します。
		/// </summary>
		void Initialize(const std::string& modelFileName, const std::string& animationFileName, bool isSkinning = true);

		/// <summary>
		/// 複数の LOD 用モデルファイルを指定して初期化します。
		/// </summary>
		/// <param name="fileName">基準となるモデルファイル名（LOD0 など）。</param>
		/// <param name="lodFiles">LOD1 以降のモデルファイルパスの配列。</param>
		/// <param name="isSkinning">true の場合はスキニング用リソースも初期化します。</param>
		void Initialize(const std::string& fileName, const std::vector<std::string>& lodFiles, bool isSkinning = true);

		/// <summary>
		/// アニメーション時間やスケルトン、スキンクラスタ、マテリアルなどを更新します。
		/// 距離によるカリングや LOD の更新間引きにも対応しています。
		/// </summary>
		void Update();

		/// <summary>
		/// 現在のWorldTransformを描画用定数バッファへ反映します。
		/// </summary>
		void RefreshWorldTransform();

		/// <summary>
		/// 単体のアニメーションモデルを描画します。
		/// Compute スキニング → Graphics の順にパイプラインを設定して描画します。
		/// </summary>
		void Draw();

		/// <summary>現在のNode AnimationまたはSkeletal PoseをShadow Mapへ描画します。</summary>
		void DrawShadow();

		/// <summary>
		/// 単一の AnimationModel をまとめて描画するユーティリティ関数です。
		/// 可視チェックも行います。
		/// </summary>
		/// <param name="models">描画対象の AnimationModel（unique_ptr）。</param>
		static void DrawBatched(const std::unique_ptr<AnimationModel>& models);

		/// <summary>
		/// AnimationModel の配列（unique_ptr）を一括で描画します。
		/// Compute パスと Graphics パスをそれぞれ 1 回ずつだけセットし、
		/// 各モデルの可視チェックを行いながら描画します。
		/// </summary>
		/// <param name="models">描画対象の AnimationModel の配列。</param>
		static void DrawBatched(const std::vector<std::unique_ptr<AnimationModel>>& models);

		/// <summary>DebugScene専用テストでCompute Skinningだけをまとめて実行し、実Dispatch数を返します。</summary>
		static size_t DispatchSkinningBatchedForDebugTest(const std::vector<std::unique_ptr<AnimationModel>>& models);
		struct DebugSharedPaletteDispatchStats
		{
			size_t sharedPaletteDispatchCount = 0;
			size_t fallbackDispatchCount = 0;
			bool sharedPaletteValid = false;
		};
		/// <summary>DebugScene専用テストで、代表Paletteを使って各モデルのCompute Skinningを実行します。</summary>
		static DebugSharedPaletteDispatchStats DispatchSkinningBatchedWithSharedPaletteForDebugTest(
			const std::vector<std::unique_ptr<AnimationModel>>& models, const AnimationModel* representative);

		/// <summary>
		/// AnimationModel の生ポインタ配列版を一括で描画します。
		/// </summary>
		/// <param name="models">描画対象の AnimationModel のポインタ配列。</param>
		static void DrawBatched(const std::vector<AnimationModel*>& models);

		/// <summary>
		/// 読み込まれたモデルデータ（メッシュ階層情報など）を取得します。
		/// </summary>
		const ModelData& GetModelData() const { return modelData; }

		bool HasMesh() const { return !modelData.subMeshes.empty(); }
		bool HasSkeleton() const { return skeleton_ && !skeleton_->GetJoints().empty(); }

		/// <summary>
		/// AnimationModel 用のデバッグ ImGui ウィンドウを描画します。
		/// LOD やスキニング状態などを確認・切り替えできます。
		/// </summary>
		void DrawImGui();

		/// <summary>
		/// 現在選択されている LOD インデックスを取得します。
		/// </summary>
		int GetLOD() const { return lodController_.GetLODIndex(); }

		/// <summary>
		/// モデルやアニメーション、関連する GPU リソースを解放し、状態を初期化します。
		/// </summary>
		void Clear();

		/// <summary>
		/// スケルトン情報をワイヤーフレームで可視化します。
		/// デバッグ用途でジョイント位置や階層を確認するときに使用します。
		/// </summary>
		void DrawSkeletonWireframe();

		/// <summary>
		/// ボディパートごとのコライダーをワイヤーフレームで描画します。
		/// デバッグ用途で当たり判定の位置・サイズを確認します。
		/// </summary>
		void DrawBodyPartColliders();

	public: /// ---------- ゲッタ ---------- ///

		/// <summary>
		/// このモデルのワールド変換情報を取得します。
		/// </summary>
		/// <returns>スケール・回転・平行移動を含む WorldTransform への const 参照。</returns>
		const WorldTransform& GetWorldTransform() const { return worldTransform; }

		/// <summary>
		/// ワールド座標系での位置（平行移動成分）を取得します。
		/// </summary>
		/// <returns>XYZ 各軸の座標。</returns>
		const Vector3& GetTranslate() const { return worldTransform.translate_; }

		/// <summary>
		/// ワールド座標系でのスケールを取得します。
		/// </summary>
		/// <returns>XYZ 各軸の拡大率。</returns>
		const Vector3& GetScale() const { return worldTransform.scale_; }

		/// <summary>
		/// ワールド座標系での回転量を取得します。（ラジアン指定想定）
		/// </summary>
		/// <returns>XYZ 各軸の回転量。</returns>
		const Vector3& GetRotate() const { return worldTransform.rotate_; }

		/// <summary>
		/// このモデルが保持している AnimationMesh へのポインタを取得します。
		/// </summary>
		/// <returns>AnimationMesh へのポインタ。未初期化時は nullptr。</returns>
		AnimationMesh* GetAnimationMesh() { return animationMesh_.get(); }

		/// <summary>
		/// 使用する LOD モデルファイルのリストを設定します。<br/>
		/// Initialize 前に呼び出しておくことで、LOD0/1/2…に対応するファイルを差し替えることができます。
		/// </summary>
		/// <param name="files">
		/// LOD 用モデルファイルパスの配列。
		/// - 推奨: LOD1 以降のみ（LOD0 は Initialize の fileName を使用）
		/// - 互換: 先頭に LOD0 を含めた「LOD0/1/2…」の全リストも可（Initialize 時に自動判定）
		/// </param>
		void SetLodFiles(const std::vector<std::string>& files) { lodSourceFiles_ = files; }

		/// <summary>
		/// 設定されている LOD モデルファイルリストをクリアします。
		/// </summary>
		void ClearLodFiles() { lodSourceFiles_.clear(); }

		/// <summary>
		/// 設定済みの LOD モデルファイルリストを取得します。
		/// </summary>
		/// <returns>LOD 用モデルファイルパスの配列。</returns>
		const std::vector<std::string>& GetLodFiles() const { return lodSourceFiles_; }

	public: /// ---------- セッタ ---------- ///

		/// <summary>
		/// ワールド座標系での位置（平行移動成分）を設定します。
		/// </summary>
		/// <param name="translate">XYZ 各軸の座標。</param>
		void SetTranslate(const Vector3& translate) { worldTransform.translate_ = translate; }

		/// <summary>
		/// ワールド座標系でのスケールを設定します。
		/// </summary>
		/// <param name="scale">XYZ 各軸の拡大率。</param>
		void SetScale(const Vector3& scale) { worldTransform.scale_ = scale; }

		/// <summary>
		/// ワールド座標系での回転量を設定します。（ラジアン指定想定）
		/// </summary>
		/// <param name="rotate">XYZ 各軸の回転量。</param>
		void SetRotate(const Vector3& rotate) { worldTransform.rotate_ = rotate; }

		/// <summary>
		/// マテリアルの反射率（スペキュラ強度）を設定します。
		/// </summary>
		void SetReflectivity(float reflectivity) { material_.SetShininess(reflectivity); }

		/// <summary>解決済みMaterialDescと5つのTexture SlotをAnimationModel共通Materialへ反映します。</summary>
		void ApplyMaterialDesc(const MaterialDesc& desc);

		/// <summary>Material Bindingを解除し、モデル読み込み時のMaterial状態へ戻します。</summary>
		void ResetMaterialBinding();

		/// <summary>
		/// 現在のワールド行列を用いて、ボディパートごとのカプセルコライダーを
		/// ワールド空間に変換した結果を取得します。
		/// </summary>
		std::vector<std::pair<std::string, Capsule>> GetBodyPartCapsulesWorld() const;

		/// <summary>
		/// 現在のワールド行列を用いて、ボディパートごとのスフィアコライダーを
		/// ワールド空間に変換した結果を取得します。
		/// </summary>
		std::vector<std::pair<std::string, Sphere>> GetBodyPartSpheresWorld() const;

		/// <summary>
		/// 頭部の描画を非表示にするかどうかを設定します（FPS視点などで使用）。
		/// </summary>
		void SetHideHead(bool hide) { hideHead_ = hide; }

		/// <summary>
		/// モデル全体に掛けるスケールファクターを設定します。
		/// </summary>
		void SetScaleFactor(float factor) { scaleFactor = factor; }

		/// <summary>
		/// モデル全体に掛かっているスケールファクターを取得します。
		/// </summary>
		float GetScaleFactor() const { return scaleFactor; }

		/// <summary>
		/// 遠距離カリングを行う際の、最終 LOD からの余裕距離を設定します。
		/// </summary>
		void SetFarCullExtra(float v) { lodController_.SetFarCullExtra(v); }

		/// <summary>
		/// 遠距離カリングされているかどうかを取得します。
		/// true の場合は描画対象外です。
		/// </summary>
		bool IsVisible() const { return !lodController_.IsCulled(); }

		/// <summary>
		/// LOD ごとの更新頻度（フレーム間引き）を設定します。
		/// 例: {1,1,2,4} → LOD2 は隔フレ、LOD3 は 4 フレに 1 回更新。
		/// </summary>
		void SetLodUpdateEvery(const std::vector<uint32_t>& v) { lodController_.SetHeavyUpdateEveryByLOD(v); }

		/// <summary>
		/// LOD を強制固定します（デバッグ用途）。
		/// enable=true の間は距離によるLOD変更を行いません。
		/// </summary>
		void SetForceLOD(bool enable, int index) { lodController_.SetForceLOD(enable, index); }

		/// <summary>
		/// 距離による LOD 切替判定を何フレームごとに行うか設定します。
		/// </summary>
		void SetLodSwitchUpdateEvery(uint32_t frames) { lodController_.SetLodSwitchUpdateEvery(frames); }

	public: /// ---------- デバッグ用アクセッサ ---------- ///

		// スケルトンを取得（デバッグ用）
		Skeleton* GetSkeleton() { return skeleton_.get(); }

		// ワールドトランスフォームを取得（デバッグ用）
		WorldTransform* GetWorldTransformPtr() { return &worldTransform; }

		// LODの取得
		const std::vector<AnimationModelLODBuilder::LODEntry>& GetLODs() const { return lods_; }

		// LODインデックスを取得
		int& GetLODIndexRef() { return lodController_.GetLODIndexRef(); }

		// ボディパートコライダー配列を取得（デバッグ・編集用）
		std::vector<BodyPartCollider>& GetBodyPartCollidersRef() { return colliderController_.GetCollidersRef(); }

		bool IsHideHead() const { return hideHead_; }

		bool IsComputeSkinningEnabled() const { return useComputeSkinning_; }
		void SetComputeSkinningEnabled(bool v) { useComputeSkinning_ = v; }
		bool PlayAnimationByIndex(uint32_t index, bool resetTime = true);
		bool PlayAnimationByName(const std::string& name, bool resetTime = true);
		bool CrossFadeAnimationByIndex(uint32_t index, float fadeDuration = 0.2f);
		bool CrossFadeAnimationByName(const std::string& name, float fadeDuration = 0.2f);
		const std::vector<AnimationLoader::AnimationClip>& GetAnimationClips() const { return animationClips_; }
		int GetCurrentAnimationIndex() const { return currentAnimationIndex_; }
		std::string GetCurrentAnimationName() const;
		int GetPreviousAnimationIndex() const { return previousAnimationIndex_; }
		std::string GetPreviousAnimationName() const;
		bool IsCrossFading() const { return isCrossFading_; }
		float GetCrossFadeTime() const { return crossFadeTime_; }
		float GetCrossFadeDuration() const { return crossFadeDuration_; }
		float GetAnimationTime() const { return animationPlayer_.GetTime(); }
		/// <summary>Debug検証などからアニメーション再生／停止だけを切り替えます。</summary>
		void SetAnimationPlaying(bool playing) { animationPlayer_.SetPlaying(playing); }
		bool IsAnimationPlaying() const { return animationPlayer_.IsPlaying(); }
		void SetAnimationSpeed(float speed) { animationPlayer_.SetSpeed(speed); }
		float GetAnimationSpeed() const { return animationPlayer_.GetSpeed(); }
		void SetAnimationLoop(bool loop) { animationPlayer_.SetLoop(loop); }
		bool IsAnimationLoop() const { return animationPlayer_.IsLoop(); }
		void ResetAnimationTime() { animationPlayer_.SetTime(0.0f); }
		/// <summary>Debug検証時だけ、スキニングWVPへProjectionまで含めます。既定OFFのため実ゲーム挙動は変えません。</summary>
		void SetUseDebugSkinningViewProjection(bool enabled) { useDebugSkinningViewProjection_ = enabled; }

		/// <summary>DebugScene専用の更新区間別CPU時間です。通常のUpdate経路では使用しません。</summary>
		struct DebugBatchUpdateTimings
		{
			float worldTransformMilliseconds = 0.0f;
			float animationTimeMilliseconds = 0.0f;
			float skeletonMilliseconds = 0.0f;
			float paletteMilliseconds = 0.0f;
			float playAnimationTimeSeconds = 0.0f;
			bool poseUpdated = false;
		};

		/// <summary>DebugScene専用負荷検証として、時刻変化または強制指定がある場合だけ姿勢を更新します。</summary>
		DebugBatchUpdateTimings UpdateForDebugBatchTest(bool playAnimation, bool forcePoseUpdate, float deltaTime);
		/// <summary>DebugSceneの検証UIへ、読み込んだアニメーション長を公開します。</summary>
		float GetAnimationDurationForDebugBatchTest() const;
		/// <summary>DebugSceneの検証UIで、アニメーションデータの読み込み成否を確認します。</summary>
		bool HasAnimationForDebugBatchTest() const;
		/// <summary>DebugScene専用テストで、論理モデルパスに対応するSources側アニメーションを再読込します。</summary>
		bool ReloadAnimationForDebugBatchTest();
		/// <summary>現在LODのPalette SRVをDebugSceneの共有姿勢検証へ公開します。</summary>
		D3D12_GPU_DESCRIPTOR_HANDLE GetCurrentPaletteSrvForDebugBatchTest() const;
		/// <summary>モデルファイルとLODが一致し、代表Paletteを安全に共有できるか確認します。</summary>
		bool CanSharePaletteWithForDebugBatchTest(const AnimationModel& representative) const;
		/// <summary>LOD不一致時の個別フォールバック用に、代表モデルの再生時刻へ同期します。</summary>
		void SetAnimationTimeForDebugBatchTest(float timeSeconds) { animationPlayer_.SetTime(timeSeconds); }
		/// <summary>DebugScene専用に、外部Paletteを使ってこのモデル固有の頂点バッファへスキニングします。</summary>
		bool DispatchSkinningCSWithExternalPaletteForDebugBatchTest(D3D12_GPU_DESCRIPTOR_HANDLE sharedPaletteSrv);

		bool IsForceLOD() const { return lodController_.IsForceLOD(); }

		int GetForcedLODIndex() const { return lodController_.GetForcedLODIndex(); }

		float GetCullDistance() const { return lodController_.GetCullDistance(); }
		void SetCullDistance(float d) { lodController_.SetCullDistance(d); }

		float GetFarCullExtra() const { return lodController_.GetFarCullExtra(); }

		int GetLodSwitchUpdateEvery() const { return (int)lodController_.GetLodSwitchUpdateEvery(); }

		float GetLodHysteresisGap() const { return (float)lodController_.GetHysteresisGap(); }
		void SetLodHysteresisGap(float g) { lodController_.SetHysteresisGap(g); }

		std::vector<float> GetLodThresholds() const { return lodController_.GetThresholds(); }
		void SetLodThresholds(const std::vector<float>& v) { lodController_.SetThresholds(v); }

		std::vector<uint32_t> GetLodUpdateEvery() const { return lodController_.GetHeavyUpdateEveryByLOD(); }

	private: /// ---------- メンバ関数 ---------- ///

		// Initialize() を見通し良くするための分割関数群
		void InitializeCommon();
		void LoadBaseModelAndAnimation();
		void CreateSkeletonFromModel();
		void InitializeMaterialAndMeshes();
		void InitializeEnvironmentMap();
		void CreateConstantBuffers();
		void BuildBodyPartColliders();
		void BuildLODsAndSkinClusters();
		void InitializeSkinningResources(bool isSkinning);
		void SetupLODControllerDefaults();
		void SyncSkinningVertexCountToCurrentLOD();

		// カメラ距離（平方距離）
		float CalcDistanceSqToCamera() const;
		const Animation* GetCurrentAnimation() const;
		Animation* GetCurrentAnimation();
		void SyncCurrentAnimationForCompatibility();
		void ClampAnimationTimeToCurrentDuration();
		const Animation* GetAnimationByIndex(int index) const;
		float AdvanceAnimationTime(float timeSeconds, float deltaTime, float duration) const;
		void ResetCrossFadeState();

		/// <summary>
		/// 現在のアニメーション時刻に基づいて、ジョイント変換などを更新します。
		/// </summary>
		void UpdateAnimation();
		void UpdateShadowParameters();
		void EnsureShadowTransformResource();
		Matrix4x4 BuildCurrentShadowWorldMatrix() const;
		bool IsShadowSkinningCacheCurrent() const;
		void UpdateShadowSkinningCacheState();

	public: /// ---------- ボーン情報の初期化 ---------- ///

		/// <summary>
		/// アニメーションメッシュ／スケルトンからボーン情報を初期化し、
		/// スキニング用データ（スキンクラスタなど）を構築します。
		/// </summary>
		void InitializeBones();

		/// <summary>
		/// カメラとの距離をもとに LOD を選択し、ヒステリシス込みで切り替えます。
		/// </summary>
		/// <param name="dist">カメラとオブジェクトの距離。</param>
		void SetLODByDistance(float dist);     // LOD選択（ヒステリシス含む）

		/// <summary>
		/// Compute Shader を使用してスキニングを実行します。
		/// t0, t1, t2, u0, b0 などのバインドを行い、頂点データを書き換えます。
		/// </summary>
		void DispatchSkinningCS();

		/// <summary>
		/// スキン済み頂点バッファを用いてモデルを描画します。
		/// </summary>
		void DrawSkinned();

	private: /// ---------- メンバ変数 ---------- ///

		WorldTransform worldTransform; // ワールド変換情報
		Material material_;            // マテリアル情報

		DirectXCommon* dxCommon_ = nullptr;  // DirectX共通クラス
		Camera* camera_ = nullptr;            // カメラ

		// 環境マップのテクスチャ
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		MaterialTextureSlots materialTextureSlots_{}; // 全LOD・SubMeshで共有する5 Texture Slot。

		ModelData modelData;    // モデルデータ
		std::string fileName_;  // 読み込んだファイル名を保持
		std::string animationFileName_; // AnimationClipを読み込むファイル名

		Animation animation; // アニメーションデータ
		std::vector<AnimationLoader::AnimationClip> animationClips_;
		int currentAnimationIndex_ = 0;
		int previousAnimationIndex_ = -1;
		bool isCrossFading_ = false;
		float crossFadeTime_ = 0.0f;
		float crossFadeDuration_ = 0.0f;
		float previousAnimationTime_ = 0.0f;

		AnimationPlayer animationPlayer_; // アニメーション再生管理

		std::unique_ptr<AnimationMesh> animationMesh_; // アニメーションメッシュ
		std::unique_ptr<Skeleton> skeleton_; // スケルトン
		SkeletonAnimator skeletonAnimator_{}; // スケルトン更新（アニメ適用）
		std::vector<std::unique_ptr<SkinCluster>> skinClusterLOD_; // LOD別

		// バッファリソースの作成
		TransformationAnimationMatrix* wvpData_ = nullptr;
		CameraForGPU* cameraData = nullptr;
		ShadowParameterForGPU* shadowParameterData_ = nullptr;
		TransformationAnimationMatrix* shadowTransformData_ = nullptr;

		ComPtr<ID3D12Resource> wvpResource; // 定数バッファ : ワールド変換行列
		ComPtr<ID3D12Resource> cameraResource; // 定数バッファ : カメラ情報
		ComPtr<ID3D12Resource> shadowParameterResource_; // Skinning PS 用 ShadowParameter
		ComPtr<ID3D12Resource> shadowTransformResource_; // Animation/Skeletal Shadow Pass用行列
		D3D12_GPU_DESCRIPTOR_HANDLE shadowMapHandle_{};

		bool hideHead_ = false; // デフォルトは表示
		float scaleFactor = 1.0f; // スケールファクター

		AnimationModelSkinningCS skinningCS_;
		bool useComputeSkinning_ = true; // UIでON/OFFするなら残す（Freeze用途）
		bool useDebugSkinningViewProjection_ = false; // DebugScene専用表示補正。実ゲームでは既定OFF。
		bool shadowSkinningPrepared_ = false;
		float shadowPreparedAnimationTime_ = 0.0f;
		float shadowPreparedCrossFadeTime_ = 0.0f;
		int shadowPreparedAnimationIndex_ = -1;
		int shadowPreparedPreviousAnimationIndex_ = -1;
		int shadowPreparedLodIndex_ = -1;
		ID3D12Resource* shadowPreparedSkinnedBuffer_ = nullptr;

		// LOD制御（距離LOD + 距離カリング + 更新間引き）
		AnimationModelLODController lodController_{};

		// LODビルダ関連
		std::vector<AnimationModelLODBuilder::LODEntry> lods_;
		std::vector<std::string> lodSourceFiles_;  // 入力
		std::vector<std::string> lodFileNames_;    // 出力
	};

} // namespace Ken4lowEngine

#include "AnimationModelShadow.inl"
