#pragma once
#include "Actor.h"
#include "PhysicsWorld.h"
#include "ActorSpawnOptions.h"
#include "ActorJsonSerializer.h"

#include <json.hpp>
#include <memory>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		  Actorを生成・所有・更新するActor用Worldクラス
	/// -------------------------------------------------------------
	class ActorWorld
	{
	public: /// ---------- テンプレート関数 ---------- ///

		/// <summary>
		/// ActorWorld内にActorを生成して登録する。
		/// </summary>
		template<class T, class... Args>
		T& SpawnActor(Args&&... args)
		{
			static_assert(std::is_base_of_v<Actor, T>, "T must inherit from Actor.");

			auto actor = std::make_unique<T>(std::forward<Args>(args)...);
			actor->SetName(actor->GetClassTypeName()); // Actorの型名をデフォルトの名前として設定する。

			Actor* spawnedActor = AddActorToWorld(std::move(actor), true);
			return *static_cast<T*>(spawnedActor); // 更新中の生成でも所有権はWorldへ移し、実体のアドレスは維持する。
		}

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ActorWorldが所有する全Actorを初期化する
		/// </summary>
		void Initialize();

		/// <summary>
		/// ActorWorldが所有する全Actorを更新する
		/// </summary>
		void Update(float deltaTime);

		/// Editor停止中に遅延要求と表示用Componentだけを安全に更新する。
		void UpdateEditor(float deltaTime);

		/// <summary>
		/// PhysicsWorld更新後に全Actorへ後処理を流す。
		/// </summary>
		void PostPhysicsUpdate(float deltaTime);

		/// <summary>
		/// ActorWorldが所有する全Actorの通常描画を行う
		/// </summary>
		void Draw();

		/// <summary>
		/// ActorWorldが所有するScreen Space UI Componentをまとめて描画する
		/// </summary>
		void DrawScreenSpaceUI();

		/// <summary>
		/// 既存呼び出しとの互換用にScreen Space UI描画へ転送する
		/// </summary>
		void DrawScreenSpaceSprites();

		/// <summary>
		/// ActorWorldが所有する全Actorのシャドウ描画を行う
		/// </summary>
		void DrawShadow();

		/// Shadow Pass前にActor Component由来の描画状態を同期する。
		void PrepareRenderState();

		/// <summary>
		/// ActorWorldが所有する全ActorのEditor表示を行う。
		/// Phase 3以降はWorld Outliner / Detailsへ統合し、既定では旧ウィンドウを表示しない。
		/// </summary>
		void DrawImGui();

		/// 現行EditorのWorld Outlinerから構成済みActor生成UIを描画する。
		void DrawActorCreationImGui() { DrawActorPrefabSpawnImGui(); }

		/// <summary>
		/// 選択中ActorまたはComponentのDetailsウィンドウを描画する。
		/// </summary>
		void DrawDetailsImGui();

		/// <summary>
		/// 統合Detailsパネル内へ選択中ActorまたはComponentの編集UIだけを描画する。
		/// </summary>
		void DrawSelectedInspectorContent();

		/// <summary>
		/// World Outlinerから選択されたActor / ComponentをActorWorld側へ同期する。
		/// </summary>
		void SetSelectedEditorObject(Actor* actor, ActorComponent* component)
		{
			if (actor && !OwnsActor(actor)) actor = nullptr;
			if (component && (!component->GetOwner() || !OwnsActor(component->GetOwner()))) component = nullptr;
			selectedActor_ = actor;
			selectedComponent_ = component;
		}

		/// <summary>
		/// 旧Actor World / Actor Detailsウィンドウを互換表示するか設定する。
		/// </summary>
		void SetLegacyEditorWindowsEnabled(bool enabled) { legacyEditorWindowsEnabled_ = enabled; }
		bool IsLegacyEditorWindowsEnabled() const { return legacyEditorWindowsEnabled_; }

		/// <summary>
		/// ActorWorldが所有する全Actorの終了処理を行う
		/// </summary>
		void Finalize();

		/// <summary>
		/// 名前からActorを検索する
		/// </summary>
		/// <param name="name">検索するActorの名前</param>
		Actor* FindActorByName(std::string_view name, bool includeInactive = true);

		/// <summary>
		/// 指定Tagを持つActorを検索する
		/// </summary>
		std::vector<Actor*> FindActorsWithTag(std::string_view tag, bool includeInactive = true);

		/// <summary>
		/// 指定Layerに所属するActorを検索する
		/// </summary>
		std::vector<Actor*> FindActorsByLayer(std::string_view layer, bool includeInactive = true);

		/// <summary>
		/// JSONファイルからActorを生成してActorWorldに追加する
		/// </summary>
		Actor* SpawnActorFromJson(std::string_view filePath, const ActorSpawnOptions& options = {});

		/// ActorをJSONへ保存する入口をActorWorldへ統一する。
		bool SaveActorToJson(const Actor& actor, std::string_view filePath);

		/// Actorの外部登録を解除してからJSONを安全に再読込する。
		bool ReloadActorFromJson(Actor& actor, std::string_view filePath);

		/// 所有Actorの削除を安全なフレーム終端へ予約する。
		bool DestroyActor(Actor* actor);

		/// <summary>Level JSON内へ埋め込まれたActor JSONからActorを生成する。</summary>
		Actor* SpawnActorFromJsonData(const nlohmann::json& actorJson, const ActorSpawnOptions& options = {})
		{
			std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(actorJson, options);
			if (!actor)
			{
				lastActorJsonSaveMessage_ = "Load failed : inline Actor JSON";
				return nullptr;
			}

			Actor* spawnedActor = actor.get();
			AddActorToWorld(std::move(actor), false); // SerializerがComponent初期化済みなのでActor固有Initializeは重ねない。

			selectedActor_ = spawnedActor;
			selectedComponent_ = nullptr;
			lastActorJsonSaveMessage_ = "Spawned : inline Actor JSON";
			return spawnedActor;
		}

	public: /// ---------- Actor一覧取得 ---------- ///

		/// <summary>
		/// ActorWorldが所有するActor一覧を取得する。
		/// </summary>
		const std::vector<std::unique_ptr<Actor>>& GetActors() const
		{
			return actors_; // Editor表示やDebug確認のために読み取り専用でActor一覧を返す
		}

		/// <summary>
		/// 選択中ActorまたはComponentの注視位置を取得する。
		/// </summary>
		bool GetSelectedFocusPosition(Vector3& outPosition) const;

	public: /// ---------- PhysicsWorld設定 ---------- ///

		/// <summary>
		/// ActorWorldで使用するPhysicsWorldを設定する
		/// </summary>
		void SetPhysicsWorld(PhysicsWorld* physicsWorld);

		/// <summary>
		/// ActorWorld用physicsWorldの初期設定を行う
		/// </summary>
		void SetupDefaultPhysicsSettings();

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// ActorWorldで使用するPhysicsWorldを設定する。
		/// </summary>
		void RegisterPhysicsComponents(Actor& actor);

		/// <summary>
		/// Actorが持つPhysics系ComponentをPhysicsWorldから登録解除する
		/// </summary>
		void UnregisterPhysicsComponents(Actor& actor);

		/// <summary>
		/// Actorが持つLightComponentを描画用ライトへ反映する
		/// </summary>
		void SyncLightComponentsToLightManager();

		/// <summary>
		/// Actor JSON読込予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingActorReload();

		/// <summary>
		/// Actor JSON Spawn予約を次フレームの安全なタイミングで処理する
		/// </summary>
		void ProcessPendingActorSpawn();

		/// <summary>
		/// Actor Prefab JSONからSpawnするためのImGuiを描画する
		/// </summary>
		void DrawActorPrefabSpawnImGui();

		/// <summary>
		/// Actor Prefabフォルダ内のJSONファイル一覧を更新する
		/// </summary>
		void RefreshActorPrefabFileList();

		/// <summary>
		/// Actor Prefabフォルダ内のJSONファイル一覧をImGuiで表示する
		/// </summary>
		void DrawActorPrefabBrowserImGui();

		/// <summary>
		/// 選択中ActorをPrefabとして保存するためのImGuiを描画する
		/// </summary>
		void DrawActorPrefabSaveImGui();

		/// <summary>
		/// 選択中ActorへComponentを追加するためのImGuiを描画する
		/// </summary>
		void DrawAddComponentImGui();

		/// <summary>
		/// 選択中Actorへ指定ClassのComponentを追加する
		/// </summary>
		void AddComponentToSelectedActor(std::string_view componentClassName);

		/// <summary>
		/// Actor内で重複しないComponent名を作成する
		/// </summary>
		std::string MakeUniqueComponentName(const Actor& actor, const std::string& baseName) const;

		/// <summary>
		/// ActorWorld内で重複しないActor名を作成する
		/// </summary>
		std::string MakeUniqueActorName(const std::string& baseName) const;

		/// <summary>
		/// 現在選択中のPrefab JSONファイルを削除する
		/// </summary>
		void DeleteSelectedActorPrefabFile();

		/// <summary>
		/// 指定パスがActor Preafabフォルダ内のJSONか確認する
		/// </summary>
		bool IsValidActorPrefabJsonPath(const std::string& filePath) const;

		/// 更新中に生成されたActorをフレーム終端でWorldへ追加する。
		void ProcessPendingActorAdds();

		/// Destroy予約済みActorを外部参照解除・Finalize後にまとめて破棄する。
		void FlushPendingDestroyedActors();

		/// ActorWorld内部に残る選択・遅延処理の参照を削除対象Actorから外す。
		void ClearReferencesToActor(Actor& actor);

		/// 指定Actorを現在のWorldが所有しているか確認する。
		bool OwnsActor(const Actor* actor) const;

		/// Actor所有権を即時または更新後キューへ追加する共通入口。
		Actor* AddActorToWorld(std::unique_ptr<Actor> actor, bool initializeActor);

		/// <summary>
		/// 選択中Componentを削除する
		/// </summary>
		void DeleteSelectedComponent();

		/// <summary>
		/// 選択中Componentを複製する
		/// </summary>
		void DuplicateSelectedComponent();

	private: /// ---------- メンバ変数 ---------- ///

		// ActorWorldがActorの寿命を管理する
		std::vector<std::unique_ptr<Actor>> actors_;

		struct PendingActorAdd
		{
			std::unique_ptr<Actor> actor;
			bool initializeActor = true;
		};

		// Actor更新中のvector変更を避けるため、生成要求をフレーム終端まで所有する。
		std::vector<PendingActorAdd> pendingActorAdds_;

		std::string lastActorJsonSaveMessage_; // ActorWorldの最後のJSON保存メッセージを保持する

		// 次フレームSpawn時に使用するオプション
		ActorSpawnOptions pendingSpawnOptions_;

		// Actor World上で選択中のActor
		Actor* selectedActor_ = nullptr;

		// JSON読込を次フレームUpdateで実行するための予約Actor
		Actor* pendingReloadActor_ = nullptr;

		// JSON読込予約中のファイルパス
		std::string pendingReloadFilePath_;

		// JSON生成予約中のファイルパス
		std::string pendingSpawnFilePath_;

		// JSON読込予約中があるかどうか
		bool hasPendingReloadActor_ = false;

		// JSON生成予約中があるかどうか
		bool hasPendingSpawnActor_ = false;

		// Actor World上で選択中のActorComponent
		ActorComponent* selectedComponent_ = nullptr;

		// Actor Detailsウィンドウへフォーカスを移す要求
		bool requestFocusActorDetails_ = false;

		// 旧Actor World / Actor Detailsウィンドウを表示する互換フラグ
		bool legacyEditorWindowsEnabled_ = false;

		// ActorWorldが所有するPhysicsWorldへの参照
		PhysicsWorld* physicsWorld_ = nullptr;

		// ActorWorldが初期化済みかどうかのフラグ
		bool isInitialized_ = false;

		// Actor列挙中のSpawnを遅延キューへ振り分けるための更新中フラグ。
		bool isUpdating_ = false;

		Vector3 actorPrefabSpawnOffset_ = { 3.0f, 0.0f, 0.0f }; // Actor Prefab Spawn時の位置オフセット

		// Editorから追加するComponentの種類選択用
		int selectedAddComponentTypeIndex_ = 0;

		// Actor Prefabフォルダ内で見つかったJSONファイル一覧
		std::vector<std::string> actorPrefabFiles_;

		// Actor Prefabフォルダのパス
		std::string actorPrefabDirectory_ = "Resources/ActorPrefabs";

		// Actor PrefabのJSONパス入力用バッファ
		std::string actorPrefabPath_ = "Resources/ActorPrefabs/TestActor.json";

		// 選択中ActorをPrefabとして保存する際のデフォルトパス
		std::string actorPrefabSavePath_ = "Resources/ActorPrefabs/NewActorPrefab.json";
	};
} // namespace Ken4lowEngine
