#pragma once
#include "SceneComponent.h"
#include "ComponentProperty.h"
#include <Collider.h>
#include <PhysicsCollisionLayer.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///	  Actorに当たり判定情報を追加するColliderComponentクラス
	/// -------------------------------------------------------------
	class ColliderComponent : public SceneComponent
	{
	public: /// ---------- コールバック型 ---------- ///

		using CollisionCallback = std::function<void(Collider*)>;
		using CollisionHitCallback = std::function<void(const CollisionHit&)>;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ColliderComponentの初期化処理
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// ColliderComponentの1フレーム更新処理
		/// </summary>
		void Update(float deltaTime) override;

		/// Editor停止中にTransform変更だけをColliderへ同期する。
		void UpdateEditor(float deltaTime) override;

		/// <summary>
		/// PhysicsWorld更新後にColliderのTransformをSceneComponentへ反映する
		/// </summary>
		void PostPhysicsUpdate(float deltaTime) override;

		/// <summary>
		/// ColliderComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// ColliderComponentの終了処理
		/// </summary>
		void Finalize() override;

	public: /// ---------- Jsonシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "ColliderComponent"; // ColliderComponentとして保存する。
		}

		/// <summary>
		/// ColliderComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからModelComponent固有情報を復元する
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- アクセサ ---------- ///

		/// <summary>
		/// Sphere判定の半径を設定する
		/// </summary>
		void SetRadius(float radius);

		/// PhysicsWorldへ登録するColliderを取得する
		Collider* GetCollider() { return collider_.get(); }
		const Collider* GetCollider() const { return collider_.get(); }

		/// <summary>
		/// Colliderの形状種別を設定する
		/// </summary>
		void SetShapeType(ECollisionShapeType shapeType);

		/// <summary>
		/// Colliderの形状種別を取得する
		/// </summary>
		ECollisionShapeType GetShapeType() const { return shapeType_; }

		/// <summary>
		/// PhysicsWorld用のCollisionLayerを設定する
		/// </summary>
		void SetCollisionLayer(uint32_t layer);

		/// <summary>
		/// PhysicsWorld用のCollisionLayerを名前付きレイヤーで設定する。
		/// </summary>
		void SetCollisionLayer(PhysicsCollisionLayer layer);

		/// <summary>
		/// AABB / OBB 判定に使う半サイズを設定する
		/// </summary>
		void SetHalfSize(const Vector3& halfSize);

		/// <summary>
		/// AABB / OBBとして扱う半サイズを取得する
		/// </summary>
		const Vector3& GetHalfSize() const { return halfSize_; }

		/// <summary>
		/// Trigger判定として扱うかを設定する
		/// </summary>
		void SetIsTrigger(bool isTrigger)
		{
			isTrigger_ = isTrigger;

			if (collider_)
			{
				collider_->SetTrigger(isTrigger_); // ColliderのTrigger設定を反映する
			}
		}

		/// Blender Level由来のcollision_type_idなど、既存識別IDをComponent経由で保持する。
		void SetCollisionTypeId(uint32_t typeId)
		{
			collisionTypeId_ = typeId;
			if (collider_) collider_->SetTypeID(collisionTypeId_);
		}
		uint32_t GetCollisionTypeId() const { return collisionTypeId_; }

		/// Blender Level由来のFloor/Obstacle/Fenceなどの用途名を保持する。
		void SetCollisionTag(std::string_view tag)
		{
			collisionTag_ = std::string(tag);
			if (collider_) collider_->SetCollisionTag(collisionTag_);
		}
		const std::string& GetCollisionTag() const { return collisionTag_; }

		std::vector<ComponentProperty> CreateProperties();

	public: /// ---------- Collision Event ---------- ///

		/// Colliderが受けた旧互換Collision通知をComponent所有者へ配送するコールバックを設定する。
		void SetOnCollisionCallback(CollisionCallback callback) { onCollision_ = std::move(callback); }
		void SetOnCollisionEnterCallback(CollisionCallback callback) { onCollisionEnter_ = std::move(callback); }
		void SetOnCollisionStayCallback(CollisionCallback callback) { onCollisionStay_ = std::move(callback); }
		void SetOnCollisionExitCallback(CollisionCallback callback) { onCollisionExit_ = std::move(callback); }

		/// 詳細Hit情報を含むBlock/Overlap通知をComponent所有者へ配送するコールバックを設定する。
		void SetOnCollisionEnterHitCallback(CollisionHitCallback callback) { onCollisionEnterHit_ = std::move(callback); }
		void SetOnCollisionStayHitCallback(CollisionHitCallback callback) { onCollisionStayHit_ = std::move(callback); }
		void SetOnCollisionExitHitCallback(CollisionHitCallback callback) { onCollisionExitHit_ = std::move(callback); }
		void SetOnOverlapBeginCallback(CollisionHitCallback callback) { onOverlapBegin_ = std::move(callback); }
		void SetOnOverlapStayCallback(CollisionHitCallback callback) { onOverlapStay_ = std::move(callback); }
		void SetOnOverlapEndCallback(CollisionHitCallback callback) { onOverlapEnd_ = std::move(callback); }

		/// Actor破棄・再構築時に古い所有者参照を残さないようCollision通知をまとめて解除する。
		void ClearCollisionCallbacks();

	private:
		class ForwardingCollider;

		void DispatchCollision(Collider* other);
		void DispatchCollisionEnter(Collider* other);
		void DispatchCollisionStay(Collider* other);
		void DispatchCollisionExit(Collider* other);
		void DispatchCollisionEnter(const CollisionHit& hit);
		void DispatchCollisionStay(const CollisionHit& hit);
		void DispatchCollisionExit(const CollisionHit& hit);
		void DispatchOverlapBegin(const CollisionHit& hit);
		void DispatchOverlapStay(const CollisionHit& hit);
		void DispatchOverlapEnd(const CollisionHit& hit);

	private: /// ---------- 内部処理 ---------- ///

		/// <summary>
		/// SceneComponentのWorld位置をCollider中心へ反映する
		/// </summary>
		void SyncFromSceneTransform();

	private: /// ---------- メンバ変数 ---------- ///

		// PhysicsWorldへ登録するCollider。Collision EventをComponentへ戻すForwardingColliderとして生成する。
		std::unique_ptr<Collider> collider_;

		// Colliderの形状種別。SphereColliderとして扱う
		ECollisionShapeType shapeType_ = ECollisionShapeType::Sphere;

		// AABB / OBBとして扱う半サイズ。デフォルトは1.0fの立方体
		Vector3 halfSize_{ 1.0f, 1.0f, 1.0f };

		// PhysicsWorld用のCollisionLayer
		uint32_t collisionLayer_ = 0;

		// Level由来の既存Collision識別情報をActor/Component化後も保持する。
		uint32_t collisionTypeId_ = 0;
		std::string collisionTag_;

		// SphereColliderとして扱う半径
		float radius_ = 1.0f;

		// Trigger判定かどうか
		bool isTrigger_ = false;

		CollisionCallback onCollision_{};
		CollisionCallback onCollisionEnter_{};
		CollisionCallback onCollisionStay_{};
		CollisionCallback onCollisionExit_{};
		CollisionHitCallback onCollisionEnterHit_{};
		CollisionHitCallback onCollisionStayHit_{};
		CollisionHitCallback onCollisionExitHit_{};
		CollisionHitCallback onOverlapBegin_{};
		CollisionHitCallback onOverlapStay_{};
		CollisionHitCallback onOverlapEnd_{};
	};
} // namespace Ken4lowEngine
