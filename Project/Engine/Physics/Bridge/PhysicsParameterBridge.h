#pragma once

#include "PhysicsWorldSettings.h"
#include "PhysicsDebugDraw.h"

#include <functional>

namespace Ken4lowEngine
{
	class PhysicsWorld;

	/// -------------------------------------------------------------
	/// Gameplay側Physicsテスト用のParameterManager設定
	/// -------------------------------------------------------------
	struct GameplayPhysicsSettings
	{
		bool enableGameplayPhysicsTest = false;
		bool enablePlayerPhysicsGroundCheck = false;
		bool enablePlayerPhysicsDepenetration = false;
		bool applyPlayerPhysicsCorrectionXZ = true;
		bool applyPlayerPhysicsCorrectionY = false;
		float playerCorrectionClamp = 1.0f;
		bool enableGameplayPhysicsTriggerTest = false;
		bool usePhysicsForPlayerStage = false;
		bool usePhysicsForPlayerGround = false;
		bool usePhysicsForPlayerDepenetration = false;
		bool usePhysicsForTriggerTest = false;
		bool usePhysicsForBulletTrigger = false;
		bool usePhysicsForEnemyStage = false;
	};

	/// -------------------------------------------------------------
	/// ParameterManagerの値をPhysicsWorldへ橋渡しするクラス
	/// -------------------------------------------------------------
	class PhysicsParameterBridge
	{
	public:
		// ParameterManagerへ物理関連グループと既定値を登録する。
		void Initialize();

		// 破棄済みインスタンスへの反映呼び出しを防ぐため、登録済みApplierを解除する。
		void Finalize(const void* owner);

		// 指定ownerに対して、物理パラメータ更新時の反映処理を登録する。
		void RegisterAppliers(const void* owner, std::function<void()> applyCallback);

		// JSON/ImGuiで調整した値を物理ワールドへ反映する。
		void ApplyTo(PhysicsWorld& physicsWorld);

		// JSON/ImGuiで調整した値を物理DebugDrawへ反映する。
		void ApplyTo(PhysicsDebugDraw& debugDraw);

		// Physics Bridge専用の補助UIを描画する。
		void DrawImGui();

		// 物理関連グループをJSONへ保存する。
		void Save();

		// 物理関連グループをJSONから読み込む。
		void Load();

		PhysicsWorldSettings GetWorldSettings() const;
		PhysicsDebugDrawSettings GetDebugDrawSettings() const;
		GameplayPhysicsSettings GetGameplaySettings() const;

	private:
		// ParameterManagerの現在値から各設定キャッシュを更新する。
		void RefreshFromParameterManager();

	private:
		PhysicsWorldSettings worldSettings_{};
		PhysicsDebugDrawSettings debugDrawSettings_{};
		GameplayPhysicsSettings gameplaySettings_{};
		bool initialized_ = false;
	};

} // namespace Ken4lowEngine
