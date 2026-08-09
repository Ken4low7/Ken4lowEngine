#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <json.hpp>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///

	// ActorComponentが所有者Actorを参照するための前方宣言
	class Actor;

	/// -------------------------------------------------------------
	/// 		  Actorに追加できる機能単位の基底クラス
	/// -------------------------------------------------------------
	class ActorComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		/// <summary>
		/// ActorComponentのデフォルトコンストラクタ。Actorに追加する際に呼ばれる。
		/// </summary>
		ActorComponent() = default;

		/// <summary>
		/// 派生ActorComponentを安全に破棄するための仮想デストラクタ。
		/// </summary>
		virtual ~ActorComponent() = default;

	public: /// ---------- 仮想関数 ---------- ///

		/// <summary>
		/// ActorComponent生成後の初期化処理
		/// </summary>
		virtual void Initialize() {}

		/// <summary>
		/// ActorComponentの1フレーム更新処理
		/// </summary>
		/// <param name="deltaTime">ゲーム時間</param>
		virtual void Update([[maybe_unused]] float deltaTime) {}

		/// <summary>
		/// Editor停止中にTransformや描画用バッファだけを更新する処理。
		/// </summary>
		virtual void UpdateEditor([[maybe_unused]] float deltaTime) {} // ゲームロジックや物理を進めずEditor表示だけを同期する。

		/// <summary>
		/// PhysicsWorld更新後に呼ばれる後処理
		/// </summary>
		virtual void PostPhysicsUpdate([[maybe_unused]] float deltaTime) {}

		/// <summary>
		/// ゲーム画面に表示する描画処理
		/// </summary>
		virtual void Draw() {}

		/// <summary>
		/// シャドウを描画する処理
		/// </summary>
		virtual void DrawShadow() {}

		/// <summary>このComponentがShadow Caster設定を持つか返す。</summary>
		virtual bool SupportsShadowCasting() const { return false; }

		/// <summary>このComponentがR32_UINT Object-ID Passへ描画できるか返す。</summary>
		virtual bool SupportsEditorObjectId() const { return false; }

		/// <summary>Editor Picking専用Passへ指定IDを書き込む形状を描画する。</summary>
		virtual void DrawEditorObjectId([[maybe_unused]] uint32_t objectId) {}

		/// <summary>
		/// EditorやDebug用のImGui描画処理
		/// </summary>
		virtual void DrawImGui() {}

		/// <summary>
		/// Detailsウィンドウに表示するComponent詳細を描画する。
		/// </summary>
		virtual void DrawInspectorImGui()
		{
#ifdef USE_IMGUI
			bool active = IsActive();
			if (ImGui::Checkbox("Active", &active))
			{
				SetActive(active); // Componentの有効状態を更新する
			}

			if (SupportsShadowCasting())
			{
				bool castShadow = IsCastShadowEnabled();
				if (ImGui::Checkbox("影を落とす", &castShadow))
				{
					SetCastShadowEnabled(castShadow); // Shadow MapへのCaster登録だけを切り替える。
				}
			}

			int updateOrder = GetUpdateOrder();
			if (ImGui::DragInt("Update Order", &updateOrder, 1.0f))
			{
				SetUpdateOrder(updateOrder); // Updateの実行順を更新する
			}

			int drawOrder = GetDrawOrder();
			if (ImGui::DragInt("Draw Order", &drawOrder, 1.0f))
			{
				SetDrawOrder(drawOrder); // Drawの実行順を更新する
			}
#endif // USE_IMGUI
			DrawImGui(); // 既存のImGui描画をDetails表示に流用する
		}

		/// <summary>
		/// ActorComponent破棄前の終了処理
		/// </summary>
		virtual void Finalize() {};

		/// ActorWorld管理下で初期化を一度だけ実行する。
		void InitializeForWorld()
		{
			if (isInitialized_) return;
			Initialize();
			isInitialized_ = true;
		}

		/// ActorWorldから外れる際に終了処理を一度だけ実行する。
		void FinalizeForWorld()
		{
			if (!isInitialized_) return;
			Finalize();
			isInitialized_ = false;
		}

		/// Componentのライフサイクル初期化が完了しているか返す。
		bool IsInitialized() const { return isInitialized_; }

	public: /// ---------- 所有者Actorの取得 / 設定 ---------- ///

		/// <summary>
		/// このActorComponentを所有するActorを設定する
		/// </summary>
		/// <param name="owner"></param>
		void SetOwner(Actor* owner)
		{
			owner_ = owner; // ActorComponentはActorを所有せず、参照だけ保持する
		}

		/// <summary>
		/// このActorComponentを所有するActorを取得する
		/// </summary>
		/// <returns></returns>
		Actor* GetOwner() const
		{
			return owner_; // ActorComponentはActorを所有せず、参照だけ保持する
		}

	public: /// ---------- 名前設定 ---------- ///

		/// <summary>
		/// Componentの識別名を設定する。
		/// </summary>
		void SetName(std::string_view name)
		{
			name_ = std::string(name); // string_viewは保持せず、Component側で文字列を所有する
		}

		/// <summary>
		/// Componentの識別名を取得する。
		/// </summary>
		const std::string& GetName() const
		{
			return name_; // Editor表示やComponent検索に使う名前を返す
		}

	public: /// ---------- 有効状態 ---------- ///

		/// <summary>
		/// Componentの有効状態を設定する
		/// </summary>
		void SetActive(bool active)
		{
			isActive_ = active; // Editor上で一時的にComponentの更新・描画を止めるためのフラグ
		}

		/// <summary>
		/// Componentが有効かどうかを取得する
		/// </summary>
		bool IsActive() const
		{
			return isActive_; // Editor上で一時的にComponentの更新・描画を止めるためのフラグ
		}

		/// <summary>
		/// 親子階層を含めたComponentの有効状態を取得する
		/// </summary>
		virtual bool IsActiveInHierarchy() const
		{
			return IsActive(); // SceneComponent以外は自分自身のActiveだけを見る
		}

		/// <summary>Shadow Mapへ影を描くか設定する。</summary>
		void SetCastShadowEnabled(bool enabled) { castShadow_ = enabled; }

		/// <summary>Shadow Mapへ影を描く設定か取得する。</summary>
		bool IsCastShadowEnabled() const { return castShadow_; }

	public: /// ---------- 実行順 ---------- ///

		/// <summary>
		/// Updateの実行順を設定する。値が小さいほど先に更新される
		/// </summary>
		void SetUpdateOrder(int order)
		{
			updateOrder_ = order; // Updateの実行順を設定する
		}

		/// <summary>
		/// Updateの実行順を取得する
		/// </summary>
		int GetUpdateOrder() const
		{
			return updateOrder_; // Updateの実行順を取得する
		}

		/// <summary>
		/// Drawの実行順を設定する。値が小さいほど先に描画される
		/// </summary>
		void SetDrawOrder(int order)
		{
			drawOrder_ = order; // Drawの実行順を設定する
		}

		/// <summary>
		/// Drawの実行順を取得する
		/// </summary>
		int GetDrawOrder() const
		{
			return drawOrder_; // Drawの実行順を取得する
		}

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentクラス名を取得する。
		/// </summary>
		virtual std::string GetClassTypeName() const
		{
			return "ActorComponent"; // Componentの型名を返す。派生クラスでオーバーライドする
		}

		/// <summary>
		/// Component共通情報をJSONへ保存する。
		/// </summary>
		virtual void ToJson(nlohmann::json& outJson)const
		{
			outJson["Name"] = GetName();               // Component名を保存する
			outJson["Class"] = GetClassTypeName();     // Componentの種類を保存する
			outJson["Type"] = "ActorComponent";       // ActorComponent系であることを保存する
			outJson["Active"] = IsActive();            // Componentの有効状態を保存する
			outJson["UpdateOrder"] = GetUpdateOrder(); // Updateの実行順を保存する
			outJson["DrawOrder"] = GetDrawOrder();     // Drawの実行順を保存する
			if (SupportsShadowCasting())
			{
				outJson["CastShadow"] = IsCastShadowEnabled(); // Shadow Caster対応Componentだけ設定を保存する。
			}
		}

		/// <summary>
		/// JSONからComponent共通情報を復元する
		/// </summary>
		virtual void FromJson(const nlohmann::json& inJson)
		{
			if (inJson.contains("Name") && inJson["Name"].is_string())
			{
				SetName(inJson["Name"].get<std::string>()); // Actor名を復元する
			}
			if (inJson.contains("Active") && inJson["Active"].is_boolean())
			{
				SetActive(inJson["Active"].get<bool>()); // Componentの有効状態を復元する
			}
			if (inJson.contains("UpdateOrder") && inJson["UpdateOrder"].is_number_integer())
			{
				SetUpdateOrder(inJson["UpdateOrder"].get<int>()); // Updateの実行順を復元する
			}
			if (inJson.contains("DrawOrder") && inJson["DrawOrder"].is_number_integer())
			{
				SetDrawOrder(inJson["DrawOrder"].get<int>()); // Drawの実行順を復元する
			}
			if (SupportsShadowCasting() && inJson.contains("CastShadow") && inJson["CastShadow"].is_boolean())
			{
				SetCastShadowEnabled(inJson["CastShadow"].get<bool>()); // 旧JSONは既定trueを維持し、新JSONだけ設定を復元する。
			}
		}

	protected: /// ---------- メンバ変数 ---------- ///

		// 所有権はActor側にあり、ActorComponent側ではdeleteしない
		Actor* owner_ = nullptr;

		// Editor上でComponentを識別するための名前
		std::string name_ = "ActorComponent";

		// falseの場合はUpdate/Drawなどの実行対象から外す
		bool isActive_ = true;

		// falseの場合はShadow MapのCaster描画だけを停止する
		bool castShadow_ = true;

		// Update実行順。小さい値ほど先に更新する
		int updateOrder_ = 0;

		// Draw実行順。小さい値ほど先に描画する
		int drawOrder_ = 0;

		// JSON再読込やPIE切替でInitializeとFinalizeが重複しないよう状態を保持する。
		bool isInitialized_ = false;
	};
} // namespace Ken4lowEngine
