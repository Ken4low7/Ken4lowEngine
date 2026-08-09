#pragma once
#include "Vector3.h"
#include "Object3D.h"
#include "Collider.h"
#include "ItemType.h"
#include "Vector4.h"

#include <memory>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///						アイテムクラス
/// -------------------------------------------------------------
class Item : public K4E::Collider
{
public: /// ---------- メンバ関数 --------- ///

	/// <summary>
	/// アイテムの初期化処理
	/// </summary>
	/// <param name="type">アイテムの種類</param>
	/// <param name="pos">アイテムの初期位置</param>
	/// <param name="healAmount">回復量</param>
	/// <param name="ammoAmount">弾薬量</param>
	/// <param name="pickupRadius">取得半径</param>
	void Initialize(ItemType type, const K4E::Vector3& pos, int healAmount = 25, int ammoAmount = 30, float pickupRadius = 2.0f);

	// 更新処理
	void Update();

	// 描画処理
	void Draw();

	// プレイヤーとの衝突判定
	bool CheckCollisionWithPlayer(const K4E::Vector3& playerPos) const;

	// プレイヤーがアイテムを取得した際の処理
	bool OnPickup(class Player& player);

	// アイテムの効果をプレイヤーに適用する
	void ApplyTo(class Player* player);

	// アイテムを取得済みとしてマークする
	void MarkCollected()
	{
		active_ = false;
		SetEnabled(false);
	}

	// アイテムの寿命をリセットする
	void SetVisualAnimationSettings(float floatHeight, float floatSpeed, float rotationSpeed);

	// アイテムの色を設定する
	void SetVisualColor(const K4E::Vector4& color);

public: /// ---------- ゲッター ---------- ///

	// アイテムが取得済みかどうかを返す
	bool IsCollected() const { return !active_; }

	// アイテムが寿命切れかどうかを返す
	bool IsExpired() const { return lifetime_ >= maxLifetime_; }

	// アイテムがアクティブかどうかを返す
	bool IsActive() const { return active_; }

	// アイテムの種類を返す
	ItemType GetType() const { return type_; }

	// 取得判定とColliderに使用する固定位置を返す
	const K4E::Vector3& GetPosition() const { return position_; }

	// 浮遊・回転する見た目と演出に使用する位置を返す
	const K4E::Vector3& GetVisualPosition() const { return visualPosition_; }

	// アイテムの取得半径を返す
	float GetPickupRadius() const { return pickupRadius_; }

	// アイテムの回復量を返す
	int GetHealAmount() const { return healAmount_; }

	// アイテムの弾薬量を返す
	int GetAmmoAmount() const { return ammoAmount_; }

public: /// ---------- オーバーライド ---------- ///

	// 旧来の衝突通知（互換用）
	void OnCollision(K4E::Collider* other) override;

	// 詳細Hit情報を受け取る将来用入口。現段階では既存Collider*イベントへ委譲して互換性を保つ。
	void OnOverlapBegin(const K4E::CollisionHit& hit) override;

	// OBBのメンバ関数のオーバーライド
	K4E::Vector3 GetCenterPosition() const override { return position_; }

	// Collider位置を変更した時は浮遊基準も同じ場所へ移す
	void SetCenterPosition(const K4E::Vector3& pos) override
	{
		position_ = pos;
		basePosition_ = pos;
		visualPosition_ = pos;
	}

	// OBBのメンバ関数のオーバーライド
	K4E::Vector3 GetOBBHalfSize() const override { return scale_; }

	// OBBのメンバ関数のオーバーライド
	void SetOBBHalfSize(const K4E::Vector3& halfSize) override { scale_ = halfSize; }

	// OBBのメンバ関数のオーバーライド
	K4E::Vector3 GetOrientation() const override { return rotation_; }

	// OBBのメンバ関数のオーバーライド
	void SetOrientation(const K4E::Vector3& rot) override { rotation_ = rot; }

private: /// ---------- メンバ変数 ---------- ///

	// ビジュアル表現用の3Dオブジェクト
	std::unique_ptr<K4E::Object3D> object3d_;

	ItemType type_ = ItemType::None; // アイテムの種類
	K4E::Vector3 position_ = {}; // Colliderと取得判定に使用する固定位置
	K4E::Vector3 visualPosition_ = {}; // 浮遊するモデルと演出に使用する位置
	bool active_ = false; // アイテムがアクティブかどうか
	float pickupRadius_ = 2.0f; // アイテムの取得半径
	int healAmount_ = 25; // アイテムの回復量
	int ammoAmount_ = 30; // アイテムの弾薬量

	K4E::Vector3 scale_ = { 0.4f, 0.4f, 0.4f }; // アイテムのスケール
	float floatTimer_ = 0.0f; // 浮遊アニメーション用のタイマー
	float floatAmplitude_ = 0.6f; // 浮遊アニメーションの振幅
	float floatSpeed_ = 4.0f; // 浮遊アニメーションの速度
	K4E::Vector3 basePosition_ = {}; // 浮遊アニメーションの基準位置
	K4E::Vector3 rotation_ = { 0.0f, 0.0f, 0.0f }; // 回転角度
	float rotationSpeed_ = 0.01f; // 回転速度
	float lifetime_ = 0.0f; // アイテムの寿命
	const float maxLifetime_ = 999.0f; // アイテムの最大寿命（999秒で事実上無限）
};
