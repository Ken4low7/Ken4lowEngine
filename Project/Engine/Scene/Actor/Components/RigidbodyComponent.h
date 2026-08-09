#pragma once
#include "ActorComponent.h"
#include "ComponentProperty.h"
#include <Rigidbody.h>

#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class SceneComponent;

	/// -------------------------------------------------------------
	///		 Actorに物理挙動を追加するRigidbodyComponentクラス
	/// -------------------------------------------------------------
	class RigidbodyComponent : public ActorComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// RigidbodyComponentの初期化処理
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// RigidbodyComponentの1フレーム更新処理
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にRigidbodyの速度をTransformへ反映する
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// RigidbodyComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// RigidbodyComponentの終了処理
		/// </summary>
		void Finalize() override;

	public: /// ---------- Jsonシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "RigidbodyComponent"; // RigidbodyComponentとして保存する。
		}

		/// <summary>
		/// RigidbodyComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからRigidbodyComponent固有情報を復元する。
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- Rigidbody取得 ---------- ///

		/// <summary>
		/// PhysicsWorldへ登録するRigidbodyを取得する
		/// </summary>
		Rigidbody* GetRigidbody() const
		{
			return rigidbody_.get(); // PhysicsWorldへ登録するため所有権なしで返す
		}

	public: /// ---------- 設定処理 ---------- ///

		/// <summary>
		/// RigidbodyのBodyTypeを設定する
		/// </summary>
		void SetBodyType(BodyType bodyType);

		/// <summary>
		/// Rigidbodyの質量を設定する
		/// </summary>
		void SetMass(float mass);

		/// <summary>
		/// Rigidbodyに重力を適用するか設定する
		/// </summary>
		void SetUseGravity(bool useGravity);

		/// <summary>
		/// Rigidbodyの速度を設定する
		/// </summary>
		void SetVelocity(const Vector3& velocity);

		/// <summary>
		/// Rigidbodyへ力を加える
		/// </summary>
		void AddForce(const Vector3& force);

		/// <summary>
		/// RigidbodyのSleep上から復帰させる
		/// </summary>
		void WakeUp();

		/// <summary>
		/// RigidbodyのSleep機能を有効にするか設定する
		/// </summary>
		void SetSleepEnabled(bool enabled);
		void SetRestitution(float restitution);
		void SetStaticFriction(float staticFriction);
		void SetDynamicFriction(float dynamicFriction);
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// 物理で動かす対象のRootComponentを取得する
		/// </summary>
		SceneComponent* GetTargetRootComponent() const;

	private: /// ---------- メンバ変数 ---------- ///

		std::unique_ptr<Rigidbody> rigidbody_; // 実際の物理挙動を管理するRigidbody
		BodyType bodyType_ = BodyType::Dynamic; // ImGui編集用に保持するBodyType
		float mass_ = 1.0f;                    // ImGui編集用に保持する質量
		bool useGravity_ = false;              // ImGui編集用に保持する重力フラグ
		Vector3 velocity_{};                   // ImGui表示・編集用に保持する速度
		bool sleepEnabled_ = false;             // ImGui編集用に保持するSleep機能の有効状態
		float restitution_ = 0.0f;              // 反発係数
		float staticFriction_ = 0.5f;           // 静止摩擦係数
		float dynamicFriction_ = 0.2f;          // 動摩擦係数
	};

}
