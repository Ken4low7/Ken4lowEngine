#pragma once
#include <string>
#include <vector>
#include <Vector3.h>
#include <Vector4.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///　　　　　　　　ボックスコライダーデータ構造体
	/// -------------------------------------------------------------
	struct ObjectColliderData
	{
		std::string type;		// コライダーの名前
		std::string collisionType; // 衝突種別名（Floor / Obstacle など）
		int collisionTypeId = -1; // 衝突種別ID（未指定時は-1）
		Vector3 center;			// 中心位置
		Vector3 size;			// サイズ
		Vector3 rotation;		// 回転（ラジアン）
		Vector3 sourceRotationDeg; // 元JSON(Blender座標)の回転（度）
		Vector3 convertedRotationDeg; // ゲーム座標へ変換後の回転（度）
		bool hasRotation = false; // JSONの回転情報が存在したか
		bool fromColliderRotation = false; // collider_rotation を読んだか（rotation より優先）
		bool enabled = false;	// 有効フラグ
	};

	/// -------------------------------------------------------------
	///　　　　　　　　スポーンポイントデータ構造体
	/// -------------------------------------------------------------
	struct SpawnProps
	{
		int wave = 0;   // ウェーブ数
		int group = 0;  // グループ数
		int count = 1;  // スポーンする敵の数

		std::string archetype;
		bool hasArchetype = false;

		std::string enemyType; // 通常ゲーム用の敵種別（Melee / MidRange、旧Legacyは読込時Melee互換）
		bool hasEnemyType = false;
	};

	struct IntroCameraProps
	{
		int order = 0;			// カメラの切り替わる順番
		float duration = 1.5f;	// カメラの切り替わる時間（秒）
		float fov = 60.0f;		// カメラのFOV（度）
		std::string targetName; // カメラの注視点となるオブジェクトの名前

		std::string interpMode = "Linear"; // 補間モード
		std::string aimMode = "Target";    // 注視点のモード
	};

	struct DeviceObjectiveProps
	{
		std::string objectiveId;
		std::string uiName;
		float activateTime = 0.0f;
	};

	struct DefenseTargetProps
	{
		std::string objectiveId;
		std::string uiName;
		int maxHp = 100;
		int startHp = 100;
		float defenseTime = 60.0f;
	};

	struct EscapePointProps
	{
		std::string objectiveId;
		std::string uiName;
		float activateTime = 0.0f;
	};

	struct BossPhaseTriggerProps
	{
		int phase = 1;
		std::string triggerType = "BossHPBelow";
		float threshold = 1.0f;
		std::string eventId;
	};

	/// -------------------------------------------------------------
	///　　　　　　　　　オブジェクトデータ構造体
	/// -------------------------------------------------------------
	struct ObjectData
	{
		std::string name;      // オブジェクトの名前
		std::string type;      // オブジェクトのタイプ
		std::string modelName; // モデル名

		Vector3 position;                 // 位置
		Vector3 rotation;                 // 回転
		Vector3 scale{ 1.0f,1.0f,1.0f };  // スケール
		Vector4 color{ 1.0f,1.0f,1.0f,1.0f }; // GPUインスタンスごとの色。未指定JSONは白を維持する。
		ObjectColliderData collider;      // コライダーデータ

		// SpawnPoint用
		SpawnProps spawnProps;      // スポーンプロパティ
		bool hasSpawnProps = false; // スポーンプロパティが有効かどうか

		// IntroCamera用
		IntroCameraProps introCameraProps;      // イントロカメラプロパティ
		bool hasIntroCameraProps = false;       // イントロカメラプロパティが有効かどうか

		// Objective系
		DeviceObjectiveProps deviceObjectiveProps;
		bool hasDeviceObjectiveProps = false;

		DefenseTargetProps defenseTargetProps;
		bool hasDefenseTargetProps = false;

		EscapePointProps escapePointProps;
		bool hasEscapePointProps = false;

		BossPhaseTriggerProps bossPhaseTriggerProps;
		bool hasBossPhaseTriggerProps = false;
	};

	/// -------------------------------------------------------------
	///　　　　　　　　　　レベルデータ構造体
	/// -------------------------------------------------------------
	struct LevelData
	{
		std::vector<ObjectData> objects; // レベル内のオブジェクトデータ
	};

} // namespace Ken4lowEngine
