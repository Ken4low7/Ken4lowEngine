#pragma once

#include "ActorComponent.h"
#include "ComponentProperty.h"
#include "Vector3.h"

#include <vector>

namespace Ken4lowEngine
{
	/// CharacterActorから移動計算を分離し、Root TransformまたはRigidbodyへ移動要求を反映する基本Movement Component。
	class CharacterMovementComponent : public ActorComponent
	{
	public:
		/// Play中だけ現在の目標速度をRoot TransformまたはRigidbody Motorへ反映する。
		void Update(float deltaTime) override;

		/// Character用移動設定をDetails上で編集・確認する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponentクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterMovementComponent"; }

		/// 移動設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから移動設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// Damage発生元から離れる方向へ即時速度を与える。Player派生は継続時間と減衰を追加できる。
		virtual void ApplyDamageKnockback(const Vector3& direction, float horizontalPower = 6.0f, float verticalPower = 2.0f);

		/// Root TransformまたはRigidbody Motorが目指す速度を設定する。
		virtual void SetVelocity(const Vector3& velocity);

		/// 目標速度を0へ戻す。
		virtual void Stop() { velocity_ = {}; }

		/// AIの群れ補正後など、更新順の後段から現在の目標速度をRigidbodyへ再反映する。
		bool ApplyMotorTargetToRigidbody(float deltaTime);

		/// Actorを持たない経路でも同じ速度積分を使えるよう、1フレームの移動量を返す。
		Vector3 CalculateDisplacement(float deltaTime) const;

		/// Character共通の+Z前方規約でRootを指定XZ方向へ滑らかに旋回させる。
		bool FaceDirectionXZ(const Vector3& direction, float rotateSpeed, float deltaTime);

		/// 現在の目標速度を返す。
		virtual const Vector3& GetVelocity() const { return velocity_; }

		/// Rigidbody Characterが目標速度へ近づくために使える最大駆動力を設定する。
		void SetMaxDriveForce(float force);

		/// 入力を止めた時に水平速度を落とすための最大制動力を設定する。
		void SetMaxBrakingForce(float force);

		float GetMaxDriveForce() const { return maxDriveForce_; }
		float GetMaxBrakingForce() const { return maxBrakingForce_; }

		/// 進行方向の低いStage障害物を検知した時だけ、Rigidbodyへ上向き速度を与える設定をまとめて適用する。
		void ConfigureAutomaticObstacleTraversal(bool enabled, float maxClimbHeight = 2.8f, float lookAheadDistance = 2.4f, float minimumJumpSpeed = 6.5f, float cooldown = 0.55f);

		bool IsAutomaticObstacleTraversalEnabled() const { return automaticObstacleTraversalEnabled_; }
		float GetAutomaticObstacleMaxClimbHeight() const { return automaticObstacleMaxClimbHeight_; }

		/// Componentの移動反映を切り替える。
		void SetMovementEnabled(bool enabled) { movementEnabled_ = enabled; }

		/// Componentの移動反映が有効か返す。
		bool IsMovementEnabled() const { return movementEnabled_; }

	protected:
		/// Rigidbodyを持たないCharacterへRoot Transform移動を適用する。
		virtual void ApplyMovement(float deltaTime);

	private:
		/// 前方の乗越可能な障害物を探し、必要な上向き速度を現在の物理速度へ加える。
		bool TryStartAutomaticObstacleTraversal(Vector3& physicalVelocity, const Vector3& targetVelocity, float deltaTime);

		/// JSONとDetailsで共有する編集プロパティ一覧を生成する。
		std::vector<ComponentProperty> CreateProperties();

	private:
		Vector3 velocity_{}; // Rigidbody使用時は現在速度ではなくMotorの目標速度を保持する。
		float maxDriveForce_ = 80.0f;
		float maxBrakingForce_ = 120.0f;
		float automaticObstacleMaxClimbHeight_ = 2.8f;
		float automaticObstacleLookAheadDistance_ = 2.4f;
		float automaticObstacleMinimumJumpSpeed_ = 6.5f;
		float automaticObstacleCooldown_ = 0.55f;
		float automaticObstacleCooldownTimer_ = 0.0f;
		bool automaticObstacleTraversalEnabled_ = false;
		bool movementEnabled_ = true;
	};
} // namespace Ken4lowEngine
