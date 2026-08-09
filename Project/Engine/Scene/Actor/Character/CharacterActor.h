#pragma once
#include "Actor.h"
#include "CharacterDamage.h"
#include "SceneComponent.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	struct CollisionHit;
	class CharacterHealthComponent;
	class CharacterMovementComponent;
	class CharacterTargetComponent;
	class CharacterColliderComponent;
	class CharacterAnimationComponent;
	class AttackComponent;

	/// Player、通常Enemy、Bossが共有するCharacter用Actorの最小基底クラス。
	class CharacterActor : public Actor
	{
	public:
		using DamageListenerId = std::uint64_t;
		using DamageListener = std::function<void(const CharacterDamageInfo&, const CharacterDamageResult&)>;
		using DeathListenerId = std::uint64_t;
		using DeathListener = std::function<void(const CharacterDeathEvent&)>;

		/// Editor InspectorがActor RootのTransformを旧WorldTransform風に読み取るための軽量Snapshot。
		struct WorldTransformSnapshot
		{
			Vector3 translate_{};
			Vector3 rotate_{};
			Vector3 scale_{ 1.0f, 1.0f, 1.0f };
		};

	public:
		/// Characterの既定Componentを不足分だけ生成し、処理本体は各Componentへ委譲する。
		void Initialize() override;

		/// 死亡通知の外部参照を解除してからActor共通終了処理を行う。
		void Finalize() override;

		/// JSON保存・復元で使用するActorクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterActor"; }

		/// HP Componentへダメージ処理を委譲し、死亡へ遷移した場合だけイベントを通知する。
		CharacterDamageResult ApplyDamage(const CharacterDamageInfo& damageInfo);
		CharacterDamageResult ApplyDamage(float amount);

		/// 最後に実際へ適用されたDamage情報を返し、被弾方向や演出の発生元解決に使う。
		const CharacterDamageInfo& GetLastAcceptedDamageInfo() const { return lastAcceptedDamageInfo_; }

		/// HP Componentへ問い合わせて現在の生存状態を返す。
		bool IsAlive() const;
		bool IsDead() const { return !IsAlive(); }

		/// Target ComponentのWorld座標を返し、未設定時はRoot座標へフォールバックする。
		Vector3 GetTargetPosition() const;

		/// Editorの読み取り専用Inspector向けにRoot TransformのSnapshotを返す。
		WorldTransformSnapshot* GetWorldTransform()
		{
			RefreshWorldTransformSnapshot();
			return &worldTransformSnapshot_;
		}
		const WorldTransformSnapshot* GetWorldTransform() const
		{
			RefreshWorldTransformSnapshot();
			return &worldTransformSnapshot_;
		}

		CharacterHealthComponent* GetHealthComponent();
		const CharacterHealthComponent* GetHealthComponent() const;
		CharacterTargetComponent* GetTargetComponent();
		const CharacterTargetComponent* GetTargetComponent() const;
		CharacterMovementComponent* GetMovementComponent();
		const CharacterMovementComponent* GetMovementComponent() const;
		CharacterColliderComponent* GetColliderComponent();
		const CharacterColliderComponent* GetColliderComponent() const;
		CharacterAnimationComponent* GetAnimationComponent();
		const CharacterAnimationComponent* GetAnimationComponent() const;
		AttackComponent* GetAttackComponent();
		const AttackComponent* GetAttackComponent() const;

		/// PhysicsWorld/Legacy CollisionManagerのどちらからも同じComponent所有Colliderを参照する。
		Collider* GetCollisionPrimitive();
		const Collider* GetCollisionPrimitive() const;

		/// Characterが所有する指定型Componentへ統一した入口からアクセスする。
		template<class T>
		T* GetCharacterComponent()
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");
			return GetComponent<T>();
		}

		/// Characterが所有する指定型Componentへ統一した入口からアクセスする。
		template<class T>
		const T* GetCharacterComponent() const
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");
			const auto components = GetComponents<T>();
			return components.empty() ? nullptr : components.front();
		}

		/// CharacterColliderComponentが受けた旧互換Collision通知を派生Characterへ配送する。
		virtual void OnCollision(Collider* other) { (void)other; }
		virtual void OnCollisionEnter(Collider* other) { OnCollision(other); }
		virtual void OnCollisionStay(Collider* other) { OnCollision(other); }
		virtual void OnCollisionExit(Collider* other) { (void)other; }

		/// 詳細Hit情報を受け取るCollision通知。未override時は旧Collider*イベントへ委譲する。
		virtual void OnCollisionEnter(const CollisionHit& hit);
		virtual void OnCollisionStay(const CollisionHit& hit);
		virtual void OnCollisionExit(const CollisionHit& hit);

		/// Trigger/Overlap通知。未override時は対応するCollision通知へ委譲する。
		virtual void OnOverlapBegin(const CollisionHit& hit);
		virtual void OnOverlapStay(const CollisionHit& hit);
		virtual void OnOverlapEnd(const CollisionHit& hit);

		DamageListenerId AddDamageListener(DamageListener listener);
		bool RemoveDamageListener(DamageListenerId listenerId);
		bool HasDamageListener(DamageListenerId listenerId) const;
		void ClearDamageListeners();
		DeathListenerId AddDeathListener(DeathListener listener);
		bool RemoveDeathListener(DeathListenerId listenerId);
		bool HasDeathListener(DeathListenerId listenerId) const;
		void ClearDeathListeners();

	protected:
		/// 派生Characterが死亡時の固有処理を追加するための拡張点。
		virtual void OnDeath(const CharacterDeathEvent& deathEvent) { (void)deathEvent; }

	private:
		/// RootComponentの現在値からEditor用Snapshotを更新する。
		void RefreshWorldTransformSnapshot() const
		{
			const SceneComponent* root = GetRootComponent();
			if (!root)
			{
				worldTransformSnapshot_ = {};
				worldTransformSnapshot_.scale_ = { 1.0f, 1.0f, 1.0f };
				return;
			}
			worldTransformSnapshot_.translate_ = root->GetWorldPosition();
			worldTransformSnapshot_.rotate_ = root->GetWorldRotation();
			worldTransformSnapshot_.scale_ = root->GetWorldScale();
		}

		/// 受理済みDamageを登録Listenerへ通知する。
		void NotifyDamageAccepted(const CharacterDamageInfo& damageInfo, const CharacterDamageResult& damageResult);
		/// 派生処理と登録Listenerへ死亡イベントを一度だけ通知する。
		void NotifyDeath(const CharacterDamageInfo& damageInfo, const CharacterDamageResult& damageResult);

	private:
		struct DamageListenerEntry
		{
			DamageListenerId id = 0;
			DamageListener listener;
		};
		struct DeathListenerEntry
		{
			DeathListenerId id = 0;
			DeathListener listener;
		};

		std::vector<DamageListenerEntry> damageListeners_;
		DamageListenerId nextDamageListenerId_ = 1;
		std::vector<DeathListenerEntry> deathListeners_;
		DeathListenerId nextDeathListenerId_ = 1;
		bool deathNotificationSent_ = false;
		CharacterDamageInfo lastAcceptedDamageInfo_{}; // 実際にHPへ入ったDamageだけを保持し、無敵・無効Damageで方向UIを出さない。
		mutable WorldTransformSnapshot worldTransformSnapshot_{};
	};
} // namespace Ken4lowEngine
