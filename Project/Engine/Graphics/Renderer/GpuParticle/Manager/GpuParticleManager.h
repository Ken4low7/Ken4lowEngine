#pragma once
#include "GpuParticleSpritePipeline.h"
#include "GpuParticleComputePipeline.h"
#include "GpuParticleBuffers.h"
#include "GpuParticleRenderer.h"
#include "GpuParticleEmitter.h"

#include "GpuParticleMeshPipeline.h"
#include "GpuParticleMeshAsset.h"

#include "GpuParticleEmitterData.h"
#include "GpuParticleEmitterAsset.h"

#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include <memory>
#include <ModelData.h>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;
	class Camera;

	/// <summary>
	/// GPUパーティクルのデバッグUIで使用する既定値・制限値をまとめた構造体です。<br/>
	/// DrawImGui() 内に直接デバッグ用の数値や文字列を埋め込まず、<br/>
	/// 既定値・入力制限値・調整値の定義場所を一元化する目的で使用します。<br/>
	/// 将来的に設定ファイルから読み込む構成へ移行する場合も、<br/>
	/// この構造体を受け皿として差し替えやすくする想定です。
	/// </summary>
	struct GpuParticleDebugPresets
	{
		/// <summary>
		/// デバッグ用 Mesh Asset 読み込みUIの既定モデルパスです。
		/// AssimpLoader 側の仕様に合わせて "Resources/Models/" からの相対指定を前提とします。
		/// </summary>
		static inline constexpr const char* kDefaultMeshModelPath = "Test/cube.gltf";

		/// <summary>
		/// デバッグ用 Mesh Asset 登録時に使用する既定のベース MeshId です。
		/// subMesh ごとに連番で登録されるため、他用途のIDと衝突しにくい開始値を与えます。
		/// </summary>
		static inline constexpr int kDefaultMeshBaseId = 1000;

		/// <summary>
		/// MeshId 入力欄の最小値です。
		/// </summary>
		static inline constexpr int kMinMeshBaseId = 0;

		/// <summary>
		/// MeshId 入力欄の最大値です。
		/// デバッグUI上で極端な値を入力しすぎないための制限値です。
		/// </summary>
		static inline constexpr int kMaxMeshBaseId = 1000000;

		/// <summary>
		/// MeshId 調整時のステップ量です。
		/// </summary>
		static inline constexpr int kMeshBaseIdStep = 1;

		/// <summary>
		/// エミッター新規作成UIで使用する既定名です。
		/// </summary>
		static inline constexpr const char* kDefaultEmitterName = "Emitter_0";

		/// <summary>
		/// エミッター新規作成UIで使用する既定テクスチャ名です。
		/// </summary>
		static inline constexpr const char* kDefaultEmitterTexture = "Effects/white.dds";

		/// <summary>
		/// エミッター新規作成UIで使用する既定半径です。
		/// </summary>
		static inline constexpr float kDefaultEmitterRadius = 0.0f;

		/// <summary>
		/// エミッター新規作成UIで使用する既定ループ回数です。
		/// </summary>
		static inline constexpr int kDefaultLoopCount = 0;

		/// <summary>
		/// エミッター新規作成UIで使用する既定ループ間隔です。
		/// </summary>
		static inline constexpr float kDefaultLoopFrequency = 0.0f;

		/// <summary>
		/// 半径調整UIのドラッグ速度です。
		/// </summary>
		static inline constexpr float kRadiusDragSpeed = 0.01f;

		/// <summary>
		/// 半径の最小値です。
		/// </summary>
		static inline constexpr float kMinRadius = 0.0f;

		/// <summary>
		/// 半径の最大値です。
		/// </summary>
		static inline constexpr float kMaxRadius = 100.0f;

		/// <summary>
		/// ループ回数調整UIのステップ量です。
		/// </summary>
		static inline constexpr int kLoopCountStep = 1;

		/// <summary>
		/// ループ回数の最小値です。
		/// </summary>
		static inline constexpr int kMinLoopCount = 0;

		/// <summary>
		/// ループ回数の最大値です。
		/// </summary>
		static inline constexpr int kMaxLoopCount = 100000;

		/// <summary>
		/// ループ周波数調整UIのドラッグ速度です。
		/// </summary>
		static inline constexpr float kLoopFrequencyDragSpeed = 0.01f;

		/// <summary>
		/// ループ周波数の最小値です。
		/// </summary>
		static inline constexpr float kMinLoopFrequency = 0.0f;

		/// <summary>
		/// ループ周波数の最大値です。
		/// </summary>
		static inline constexpr float kMaxLoopFrequency = 10.0f;

		/// <summary>
		/// 選択中エミッター編集UIで使用する半径調整速度です。
		/// </summary>
		static inline constexpr float kSelectedRadiusDragSpeed = 0.01f;

		/// <summary>
		/// 選択中エミッター編集UIで使用するループ回数のステップ量です。
		/// </summary>
		static inline constexpr int kSelectedLoopCountStep = 1;

		/// <summary>
		/// 選択中エミッター編集UIで使用するループ周波数調整速度です。
		/// </summary>
		static inline constexpr float kSelectedLoopFrequencyDragSpeed = 0.01f;

		/// <summary>
		/// バースト発生UIの既定発生数です。
		/// </summary>
		static inline constexpr int kDefaultBurstCount = 50;

		/// <summary>
		/// バースト発生UIの既定反復回数です。
		/// </summary>
		static inline constexpr int kDefaultBurstRepeat = 1;

		/// <summary>
		/// バースト発生数調整UIのステップ量です。
		/// </summary>
		static inline constexpr int kBurstCountStep = 1;

		/// <summary>
		/// バースト発生数の最小値です。
		/// </summary>
		static inline constexpr int kMinBurstCount = 0;

		/// <summary>
		/// バースト発生数の最大値です。
		/// </summary>
		static inline constexpr int kMaxBurstCount = 100000;

		/// <summary>
		/// バースト反復回数調整UIのステップ量です。
		/// </summary>
		static inline constexpr int kBurstRepeatStep = 1;

		/// <summary>
		/// バースト反復回数の最小値です。
		/// </summary>
		static inline constexpr int kMinBurstRepeat = 1;

		/// <summary>
		/// バースト反復回数の最大値です。
		/// </summary>
		static inline constexpr int kMaxBurstRepeat = 1000;
	};


	/// <summary>
	/// GPU パーティクル全体を管理するマネージャークラスです。<br/>
	/// パイプライン・バッファ・レンダラー・エミッター群・Mesh Particle Asset 群を保持し、<br/>
	/// 初期化・更新・描画・デバッグ編集の入口をまとめて提供します。<br/>
	/// 実際のシミュレーション更新は Compute Shader の Dispatch により GPU 上で実行し、<br/>
	/// CPU 側ではエミッター設定の収集や描画要求の制御を主に担当します。
	/// </summary>
	class GpuParticleManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// GpuParticleManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>GpuParticleManager の唯一のインスタンス。</returns>
		static GpuParticleManager* GetInstance();

		/// <summary>
		/// GPU パーティクルシステムの初期化処理を行います。<br/>
		/// カメラ参照の保持、各種パイプライン生成、バッファ生成、レンダラー生成を行い、<br/>
		/// 最後に初期状態のパーティクルバッファを構築するための Dispatch() を実行します。
		/// </summary>
		/// <param name="camera">ビュー・射影情報を参照するためのカメラ。</param>
		void Initialize(Camera* camera);

		/// <summary>
		/// GPU パーティクルシステムの終了処理を行います。<br/>
		/// エミッター一覧・MeshAsset 一覧・各種パイプライン・バッファ・レンダラーを解放し、<br/>
		/// 保持しているカメラ参照も無効化します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// 毎フレームの更新処理を行います。<br/>
		/// GPU バッファ側の時間情報やカメラ情報を更新したあと、<br/>
		/// GPU 上の全パーティクル更新 Dispatch を実行します。<br/>
		/// その後、各エミッターの CB を構築し、必要ならエミット用 Dispatch を発行します。
		/// </summary>
		/// <param name="deltaTime">前フレームからの経過時間（秒）。</param>
		void Update(float deltaTime);

		/// <summary>
		/// GPU パーティクルの描画処理を行います。<br/>
		/// 登録済みエミッターごとに DrawType やテクスチャを設定し、<br/>
		/// GpuParticleRenderer を通じてインスタンシング描画します。
		/// </summary>
		void Draw();

		/// <summary>Preview Statusから共通RuntimeのUpdate経路通過を確認する診断カウンタです。</summary>
		uint64_t GetUpdateCallCount() const { return updateCallCount_; }
		/// <summary>Preview Statusから共通RuntimeのDraw経路通過を確認する診断カウンタです。</summary>
		uint64_t GetDrawCallCount() const { return drawCallCount_; }
		/// <summary>発生要求がComputeのEmit Dispatchまで届いた回数を返します。</summary>
		uint64_t GetEmitDispatchCount() const { return emitDispatchCount_; }

		/// <summary>
		/// GPU パーティクル用の ImGui デバッグUIを描画します。<br/>
		/// Mesh Asset の読み込み・エミッターの作成/編集/削除・バースト発生・
		/// デバッグカメラ切り替えなどを行うための開発用UIです。
		/// </summary>
		void DrawImGui();

	public: /// ---------- メッシュパーティクル関連 ---------- ///

		/// <summary>
		/// meshId をキーに MeshParticleAsset を登録します。<br/>
		/// overwrite=true の場合は既存エントリを上書きします。
		/// </summary>
		/// <param name="meshId">登録先の識別ID。</param>
		/// <param name="asset">登録するメッシュ資産。</param>
		/// <param name="overwrite">既存登録を上書きするかどうか。</param>
		/// <returns>登録に成功した場合は true、上書き禁止かつ既存がある場合は false。</returns>
		bool RegisterMeshAsset(uint32_t meshId, MeshParticleAsset asset, bool overwrite = true);

		/// <summary>Debug Previewが所有するMesh Assetを個別に安全に解除します。</summary>
		bool UnregisterMeshAsset(uint32_t meshId);

		/// <summary>登録済みMesh Assetの表示テクスチャを安全に差し替えます。</summary>
		bool SetMeshAssetTexturePath(uint32_t meshId, const std::string& texturePath);

		/// <summary>
		/// 登録済みの MeshParticleAsset を検索します。<br/>
		/// 見つからない場合は nullptr を返します。
		/// </summary>
		/// <param name="meshId">検索対象の識別ID。</param>
		/// <returns>見つかった MeshParticleAsset へのポインタ。存在しない場合は nullptr。</returns>
		const MeshParticleAsset* FindMeshAsset(uint32_t meshId) const;

		/// <summary>
		/// AssimpLoader を使ってモデルを読み込み、subMesh ごとに
		/// (baseMeshId + i) の連番で MeshParticleAsset を登録します。<br/>
		/// 1つのモデルから複数 subMesh が生成される場合、それぞれを個別に扱えるようにします。
		/// </summary>
		/// <param name="baseMeshId">subMesh 登録開始時のベースID。</param>
		/// <param name="modelFilePath">読み込むモデルファイルパス。</param>
		/// <param name="loadTextures">テクスチャも同時にロードするかどうか。</param>
		/// <returns>少なくとも1つ以上の有効 subMesh を登録できた場合は true。</returns>
		bool LoadMeshAssetsFromAssimp(uint32_t baseMeshId, const std::string& modelFilePath, bool loadTextures = true);

		/// <summary>
		/// 登録済みの MeshAsset をすべて削除します。
		/// </summary>
		void ClearMeshAssets();

		/// <summary>
		/// デバッグ表示用に登録済み MeshAsset テーブル全体を参照します。
		/// </summary>
		const std::unordered_map<uint32_t, MeshParticleAsset>& GetMeshAssets() const { return meshAssets_; }

	public: /// ---------- エミッター関連 ---------- ///

		/// <summary>
		/// 新しい GPU パーティクルエミッターを作成し、名前をキーに内部登録します。<br/>
		/// すでに同名のエミッターが存在する場合は作成しません。
		/// </summary>
		/// <param name="name">エミッター名（識別キー）。</param>
		/// <param name="info">エミッターの初期設定情報。</param>
		/// <returns>作成成功時はエミッターへのポインタ、失敗時は nullptr。</returns>
		GpuParticleEmitter* CreateEmitter(const std::string& name, const GpuParticleEmitter::EmitterInfo& info);

		/// <summary>
		/// Sprite 用の既定プリセットから GPU パーティクルエミッターを作成します。<br/>
		/// GpuParticleEmitterPresetTable に登録された type 別既定値を使って
		/// EmitterInfo を自動生成する簡易APIです。
		/// </summary>
		/// <param name="name">エミッター名（識別キー）。</param>
		/// <param name="type">作成したい Sprite パーティクル種別。</param>
		/// <returns>作成成功時はエミッターへのポインタ、失敗時は nullptr。</returns>
		GpuParticleEmitter* CreateEmitter(const std::string& name, GpuParticleType type);

		/// <summary>
		/// Sprite 用の既定プリセットからエミッターを作成し、初期位置も設定します。
		/// </summary>
		/// <param name="name">エミッター名。</param>
		/// <param name="type">Sprite パーティクル種別。</param>
		/// <param name="position">初期配置位置。</param>
		/// <returns>作成成功時はエミッターへのポインタ、失敗時は nullptr。</returns>
		GpuParticleEmitter* CreateEmitter(const std::string& name, GpuParticleType type, const Vector3& position);

		/// <summary>
		/// 指定名のエミッターを取得します。<br/>
		/// 見つからない場合は nullptr を返します。
		/// </summary>
		/// <param name="name">取得したいエミッター名。</param>
		/// <returns>エミッターへのポインタ。存在しない場合は nullptr。</returns>
		GpuParticleEmitter* GetEmitter(const std::string& name);

		/// <summary>
		/// 指定エミッターへ一度だけ count 個ぶんの発生要求を出します。<br/>
		/// 実際の発生処理は次回 Update() 内の BuildCB() / DispatchEmit() で反映されます。
		/// </summary>
		/// <param name="name">対象エミッター名。</param>
		/// <param name="count">発生要求数。</param>
		void BurstEmitter(const std::string& name, uint32_t count);

		/// <summary>
		/// 既定プリセットのエミッターを必要に応じて作成し、指定位置でバースト発生させます。<br/>
		/// 同名エミッターが既に存在する場合は再利用します。
		/// </summary>
		/// <param name="name">エミッター名。</param>
		/// <param name="type">Sprite パーティクル種別。</param>
		/// <param name="position">発生位置。</param>
		/// <param name="count">バースト発生数。</param>
		/// <returns>利用したエミッターへのポインタ。失敗時は nullptr。</returns>
		GpuParticleEmitter* EmitBurst(const std::string& name, GpuParticleType type, const Vector3& position, uint32_t count);

		/// <summary>
		/// Runtime API 用の一時エミッターを作成します。<br/>
		/// 編集用エミッターと異なり ParameterManager へ登録せず、SaveAllEmittersToDirectory() の保存対象にも含めません。<br/>
		/// GameObject / Scene から直接呼ぶ用途ではなく、EffectSystem 経由で使用するための境界です。
		/// </summary>
		GpuParticleEmitter* CreateRuntimeEmitter(const std::string& name, const GpuParticleEmitter::EmitterInfo& info);

		/// <summary>
		/// 指定名のエミッターが Runtime API により作られた一時エミッターかどうかを返します。
		/// </summary>
		bool IsRuntimeEmitter(const std::string& name) const;

		/// <summary>
		/// デバッグカメラの有効/無効を切り替えます。<br/>
		/// GPU パーティクルバッファ側へも状態を反映します。
		/// </summary>
		/// <param name="enabled">有効にする場合は true。</param>
		void SetDebugCameraEnabled(bool enabled) { gpuParticleBuffers_->SetDebugCameraEnabled(enabled); }

		/// デバッグ表示用：登録済みエミッター数を返す。
		size_t GetEmitterCount() const { return emitters_.size(); }

		/// デバッグ表示用：寿命推定上まだ描画対象のエミッター数を返す。
		size_t GetActiveEmitterCount() const;

		/// デバッグ表示用：GPU readback なしの推定生存パーティクル数を返す。
		uint32_t GetEstimatedActiveParticleCount() const;

		/// デバッグ表示用：直近フレームの GPU Particle draw call 数を返す。
		uint32_t GetLastDrawCallCount() const { return lastDrawCallCount_; }

	public:

		bool RemoveEmitter(const std::string& name);

		/// <summary>
		/// GPUパーティクルの調整値をParameterManagerへ登録します。<br/>
		/// グループ名は "GPUParticle/エミッター名" にして、エミッター単位でJson保存できるようにします。
		/// 現状はManager内にController責務を同居させていますが、将来はLightParameterControllerと同じ
		/// "登録 / 読み込み / 反映 / 解除" 専用クラスへ切り出せる境界としてこの3関数に集約します。
		/// </summary>
		void RegisterEmitterParameters(GpuParticleEmitter& emitter);

		/// <summary>
		/// ParameterManagerで編集・読み込みされた値を、対象エミッターへ反映します。
		/// </summary>
		void ApplyEmitterParameters(GpuParticleEmitter& emitter);

		/// <summary>
		/// エミッター破棄前にParameterManagerの反映コールバックを解除します。
		/// </summary>
		void UnregisterEmitterParameters(const GpuParticleEmitter& emitter);

		GpuParticleEmitterAsset BuildAssetFromEmitter(const std::string& name) const;
		GpuParticleEmitter* CreateEmitterFromAsset(const GpuParticleEmitterAsset& asset, bool overwrite = true);

		bool SaveEmitterToFile(const std::string& name, const std::string& filePath) const;
		GpuParticleEmitter* LoadEmitterFromFile(const std::string& filePath, bool overwrite = true);

		void LoadEmittersFromDirectory(const std::string& directoryPath, bool overwrite = true);
		bool SaveAllEmittersToDirectory(const std::string& directoryPath) const;

	private: /// ---------- ディスパッチ関数 ---------- ///

		/// <summary>
		/// 初期化用の Compute Dispatch を実行します。<br/>
		/// パーティクルバッファを初期状態へセットアップする用途で使用します。
		/// </summary>
		void Dispatch();

		/// <summary>
		/// エミット専用の Compute Dispatch を実行します。<br/>
		/// 指定エミッターCBを参照し、新規パーティクル発生処理のみをGPU上で行います。
		/// </summary>
		/// <param name="emitterCbAddr">エミッター定数バッファのGPU仮想アドレス。</param>
		void DispatchEmit(D3D12_GPU_VIRTUAL_ADDRESS emitterCbAddr);

		/// <summary>
		/// 毎フレーム更新用の Compute Dispatch を実行します。<br/>
		/// 既存パーティクルの位置・寿命・速度などを GPU 上で更新します。
		/// </summary>
		void DispatchUpdate();

		/// <summary>
		/// 読み込んだ subMesh から MeshParticleAsset を生成します。<br/>
		/// DefaultHeap 上に VB/IB を作成し、必要ならテクスチャもロードします。
		/// </summary>
		/// <param name="subMesh">変換元の subMesh。</param>
		/// <param name="loadTexture">テクスチャ読み込みを行うかどうか。</param>
		/// <returns>生成された MeshParticleAsset。</returns>
		MeshParticleAsset CreateMeshAssetFromSubMesh(const SubMesh& subMesh, bool loadTexture);

		static std::string BuildEmitterJsonPath(const std::string& directoryPath, const std::string& emitterName);
		static std::string BuildEmitterParameterGroupName(const std::string& emitterName);
		std::string MakeUniqueEmitterName(const std::string& baseName) const;
		GpuParticleEmitter* CreateEmitterInternal(const std::string& name, const GpuParticleEmitter::EmitterInfo& info, bool registerParameters);

	private: /// ---------- メンバ変数 ---------- ///

		/// <summary>
		/// カメラ参照です。
		/// ビュー行列・射影行列など、GPU パーティクル更新や描画に必要な情報取得に使用します。
		/// </summary>
		Camera* camera_ = nullptr;

		/// <summary>
		/// デバッグカメラの有効状態です。
		/// </summary>
		bool isDebugCamera_ = false;

		/// <summary>
		/// GPU スプライト描画用パイプラインです。
		/// </summary>
		std::unique_ptr<GpuParticleSpritePipeline> spritePipeline_;

		/// <summary>
		/// Compute Shader 実行用パイプラインです。
		/// 初期化・更新・エミット用の PSO / RootSignature を扱います。
		/// </summary>
		std::unique_ptr<GpuParticleComputePipeline> computePipeline_;

		/// <summary>
		/// GPU パーティクルシミュレーション用バッファ群です。
		/// </summary>
		std::unique_ptr<GpuParticleBuffers> gpuParticleBuffers_;

		/// <summary>
		/// GPU パーティクル描画処理を担当するレンダラーです。
		/// </summary>
		std::unique_ptr<GpuParticleRenderer> gpuParticleRenderer_;

		/// <summary>
		/// 名前をキーにしたエミッター管理テーブルです。
		/// </summary>
		std::unordered_map<std::string, std::unique_ptr<GpuParticleEmitter>> emitters_;

		/// <summary>
		/// Runtime API が作成した一時エミッター名です。<br/>
		/// 編集用 Save / ParameterManager 登録の対象から外すため、通常エミッターと区別します。
		/// </summary>
		std::unordered_set<std::string> runtimeEmitterNames_;

		uint32_t lastDrawCallCount_ = 0;
		uint64_t updateCallCount_ = 0;
		uint64_t drawCallCount_ = 0;
		uint64_t emitDispatchCount_ = 0;

	private: /// ---------- メッシュデータ ---------- ///

		/// <summary>
		/// Mesh Particle 描画用パイプラインです。
		/// </summary>
		std::unique_ptr<GpuParticleMeshPipeline> meshPipeline_;

		/// <summary>
		/// meshId をキーにした MeshParticleAsset 管理テーブルです。
		/// </summary>
		std::unordered_map<uint32_t, MeshParticleAsset> meshAssets_;
	};

	/// <summary>
	/// ゲーム側から GPU パーティクルを安全に呼び出すための Runtime API 入口です。<br/>
	/// GameObject / Scene は GpuParticleManager の emitter map や編集用 ParameterManager には触れず、<br/>
	/// 登録済みの runtime effectName と座標だけで単発再生・ループ再生・停止を行います。<br/>
	/// 現段階では Sprite 系エフェクトを安定対象とし、Mesh / Ribbon / Beam は既存実装を壊さないため未対応境界として扱います。
	/// </summary>
	class EffectSystem
	{
	public:
		/// Runtime 再生を個別停止するための軽量ハンドルです。
		struct PlayHandle
		{
			uint32_t id = 0;
			bool IsValid() const { return id != 0; }
		};

		/// <summary>
		/// EffectSystem のシングルトンインスタンスを取得します。
		/// </summary>
		static EffectSystem* GetInstance();

		/// <summary>
		/// effectName に対応する Sprite エフェクトを position で一度だけ再生します。
		/// </summary>
		PlayHandle Play(const std::string& effectName, const Vector3& position);

		/// <summary>
		/// effectName に対応する Sprite エフェクトを position で一度だけ再生し、発生数だけ呼び出し側で上書きします。
		/// </summary>
		PlayHandle Play(const std::string& effectName, const Vector3& position, uint32_t emitCount);

		/// <summary>
		/// effectName に対応する Sprite エフェクトを position でループ再生します。<br/>
		/// 同じ effectName がすでにループ中なら位置を更新して既存ハンドルを返します。
		/// </summary>
		PlayHandle PlayLoop(const std::string& effectName, const Vector3& position);

		/// <summary>
		/// effectName で開始したループ再生を停止します。
		/// </summary>
		bool Stop(const std::string& effectName);

		/// <summary>
		/// Play / PlayLoop が返したハンドルに対応する runtime エフェクトを停止します。
		/// </summary>
		bool Stop(PlayHandle handle);

		/// <summary>
		/// Runtime 再生名と Sprite 種別の対応を登録します。<br/>
		/// 既存の編集用 emitter 名とは分け、"PlayerHit" などゲーム側の意味名を固定するために使います。
		/// </summary>
		void RegisterSpriteEffect(const std::string& effectName, GpuParticleType spriteType, uint32_t defaultEmitCount, uint32_t loopEmitCount = 0, float loopFrequency = 0.0f, float radiusOverride = -1.0f);

		/// <summary>
		/// Runtime API の状態と未対応モードの境界を ImGui へ表示します。
		/// </summary>
		void DrawImGui();

	private:
		struct RuntimeEffectDefinition
		{
			GpuParticleType spriteType = GpuParticleType::Default;
			uint32_t defaultEmitCount = 1;
			uint32_t loopEmitCount = 0;
			float loopFrequency = 0.0f;
			float radiusOverride = -1.0f;
		};

		struct RuntimeInstance
		{
			PlayHandle handle{};
			std::string effectName;
			std::string emitterName;
			bool loop = false;
		};

		EffectSystem();

		void RegisterDefaultSpriteEffects();
		PlayHandle PlayInternal(const std::string& effectName, const Vector3& position, uint32_t emitCount, bool loop);
		const RuntimeEffectDefinition* FindDefinition(const std::string& effectName);
		static std::string BuildRuntimeEmitterName(const std::string& effectName);
		void SetStatus(bool succeeded, const std::string& message);
		PlayHandle AllocateHandle();

		std::unordered_map<std::string, RuntimeEffectDefinition> definitions_;
		std::unordered_map<std::string, RuntimeInstance> activeLoopsByEffectName_;
		std::unordered_map<uint32_t, std::string> handleToEffectName_;
		uint32_t nextHandleId_ = 1;
		bool defaultsRegistered_ = false;
		bool lastOperationSucceeded_ = true;
		std::string lastStatus_ = "Ready";
	};

} // namespace Ken4lowEngine
