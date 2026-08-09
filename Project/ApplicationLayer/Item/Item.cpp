#define NOMINMAX
#include "Item.h"
#include "Player.h"
#include <CollisionPreset.h>
#include <CollisionTypeIdDef.h>
#include <GameTimer.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>

using namespace Ken4lowEngine;

namespace
{
	// アイテムの種類に応じたモデルパスを返す
	const char* GetModelPath(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall:
		case ItemType::AmmoSmall:
		case ItemType::NextStageKey:
		default:
			return "Sample/cube.gltf";
		}
	}
}

/// -------------------------------------------------------------
///						アイテムの初期化処理
/// -------------------------------------------------------------
void Item::Initialize(ItemType type, const K4E::Vector3& pos, int healAmount, int ammoAmount, float pickupRadius)
{
	ApplyCollisionPreset(*this, ECollisionPresetId::Item); // Preset適用テストとして、従来のkItem TypeIDと同じ設定を反映する。
	SetOwner<Item>(this);
#ifdef _DEBUG
	const uint32_t legacyItemTypeId = static_cast<uint32_t>(CollisionTypeIdDef::kItem);
	assert(K4E::Collider::GetTypeID() == legacyItemTypeId && "Item preset must keep legacy kItem TypeID.");
#endif
	K4E::Collider::SetOBBHalfSize(scale_);

	type_ = type;
	position_ = pos;
	basePosition_ = pos;
	visualPosition_ = pos;
	active_ = (type_ != ItemType::None);
	SetEnabled(active_);
	pickupRadius_ = pickupRadius;
	healAmount_ = healAmount;
	ammoAmount_ = ammoAmount;
	lifetime_ = 0.0f;
	floatTimer_ = 0.0f;
	rotation_ = { 0.0f, 0.0f, 0.0f };

	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize(GetModelPath(type_));
	object3d_->SetTranslate(visualPosition_);
	object3d_->SetScale(scale_);

	switch (type_)
	{
	case ItemType::HealSmall:
		object3d_->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
		break;
	case ItemType::AmmoSmall:
		object3d_->SetColor({ 0.0f, 0.0f, 1.0f, 1.0f });
		break;
	case ItemType::NextStageKey:
		object3d_->SetColor({ 0.0f, 1.0f, 1.0f, 1.0f });
		break;
	case ItemType::None:
	default:
		object3d_->SetColor({ 1.0f, 1.0f, 1.0f, 0.5f });
		break;
	}
}

/// -------------------------------------------------------------
/// 			アイテムのビジュアルアニメーション設定
/// -------------------------------------------------------------
void Item::SetVisualAnimationSettings(float floatHeight, float floatSpeed, float rotationSpeed)
{
	floatAmplitude_ = std::max(0.0f, floatHeight);
	floatSpeed_ = std::max(0.0f, floatSpeed);
	rotationSpeed_ = rotationSpeed;
}

/// -------------------------------------------------------------
/// 			アイテムのビジュアルカラー設定
/// -------------------------------------------------------------
void Item::SetVisualColor(const K4E::Vector4& color)
{
	if (object3d_)
	{
		object3d_->SetColor(color);
	}
}

/// -------------------------------------------------------------
///						アイテムの更新処理
/// -------------------------------------------------------------
void Item::Update()
{
	// 非アクティブ状態なら更新処理をスキップ
	if (!active_) return;

	const float deltaTime = GameTimer::GetInstance()->GetDeltaTime();

	lifetime_ += deltaTime;
	floatTimer_ += floatSpeed_ * deltaTime;
	const float floatOffset = std::sinf(floatTimer_) * floatAmplitude_;
	visualPosition_ = basePosition_;
	visualPosition_.y += floatOffset; // 見た目だけを浮遊させ、取得Colliderは床付近の固定位置に残す。
	rotation_.y += rotationSpeed_ * deltaTime;

	if (object3d_)
	{
		object3d_->SetTranslate(visualPosition_);
		object3d_->SetRotate(rotation_);
		object3d_->Update();
	}

	K4E::Collider::SetCenterPosition(position_);
}

/// -------------------------------------------------------------
///					   アイテムの描画処理
/// -------------------------------------------------------------
void Item::Draw()
{
	if (active_ && object3d_)
	{
		object3d_->Draw();
	}
}

/// -------------------------------------------------------------
///					 プレイヤーとの衝突判定
/// -------------------------------------------------------------
bool Item::CheckCollisionWithPlayer(const K4E::Vector3& playerPos) const
{
	if (!active_) return false;
	const K4E::Vector3 diff = position_ - playerPos;
	return K4E::Vector3::Length(diff) <= pickupRadius_;
}

/// -------------------------------------------------------------
///				プレイヤーがアイテムを取得した際の処理
/// -------------------------------------------------------------
bool Item::OnPickup(Player& player)
{
	if (!active_) return false;

	switch (type_)
	{
	case ItemType::HealSmall:
		player.Heal(static_cast<float>(healAmount_));
		break;
	case ItemType::AmmoSmall:
		player.AddReserveAmmo(ammoAmount_);
		break;
	case ItemType::NextStageKey:
	case ItemType::None:
	default:
		break;
	}

	MarkCollected();
	return true;
}

/// -------------------------------------------------------------
///				アイテムの効果をプレイヤーに適用する
/// -------------------------------------------------------------
void Item::ApplyTo(Player* player)
{
	if (!player) return;
	(void)OnPickup(*player);
}

/// -------------------------------------------------------------
///					アイテムの衝突判定
/// -------------------------------------------------------------
void Item::OnCollision(K4E::Collider* other)
{
	if (!other) return;

	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
	{
		// アイテム効果は ItemManager::ApplyItemEffect に集約し、当たり判定コールバックでは消費しない。
		return;
	}
}

/// -------------------------------------------------------------
///					アイテムのOverlap開始通知
/// -------------------------------------------------------------
void Item::OnOverlapBegin(const K4E::CollisionHit& hit)
{
	// ItemはPreset上Overlap扱い。取得効果はItemManager::ApplyItemEffectへ集約し、ここでは通知確認だけに留める。
	OnCollision(hit.other);
}
