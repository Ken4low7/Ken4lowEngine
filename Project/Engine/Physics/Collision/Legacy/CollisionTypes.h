#pragma once
#include <cstdint>

#include "CollisionTypeIdDef.h"

/// UEのObject Channelへ段階移行するため、既存TypeIDと同じ数値で対応させる。
enum class EObjectChannel : uint32_t
{
	Default = static_cast<uint32_t>(CollisionTypeIdDef::kDefault),
	Player = static_cast<uint32_t>(CollisionTypeIdDef::kPlayer),
	Weapon = static_cast<uint32_t>(CollisionTypeIdDef::kWeapon),
	Enemy = static_cast<uint32_t>(CollisionTypeIdDef::kEnemy),
	PlayerProjectile = static_cast<uint32_t>(CollisionTypeIdDef::kBullet),
	EnemyProjectile = static_cast<uint32_t>(CollisionTypeIdDef::kEnemyBullet),
	Item = static_cast<uint32_t>(CollisionTypeIdDef::kItem),
	Dummy = static_cast<uint32_t>(CollisionTypeIdDef::kDummy),
	Boss = static_cast<uint32_t>(CollisionTypeIdDef::kBoss),
	BossProjectile = static_cast<uint32_t>(CollisionTypeIdDef::kBossBullet),
	WorldStatic = static_cast<uint32_t>(CollisionTypeIdDef::kWorld),
	TargetLock = static_cast<uint32_t>(CollisionTypeIdDef::kTragetLock), // 既存名のtypoは互換のため残す。
	Crystal = static_cast<uint32_t>(CollisionTypeIdDef::kCrystal),

	Count = 32
};

/// UEのIgnore / Overlap / Blockに相当する衝突反応の定義。
enum class ECollisionResponse : uint8_t
{
	Ignore,  // 判定・イベント対象から外す。
	Overlap, // すり抜けるが接触イベント対象にする予定地。
	Block    // 物理的に遮る接触として扱う予定地。
};

/// UEのTrace Channel相当で、Raycast/SegmentCastなどの問い合わせ目的を表す。
enum class ETraceChannel : uint8_t
{
	Visibility,
	Camera,
	Weapon,
	AI,
	Interaction,

	Count
};

/*
CollisionTypeIdDef と EObjectChannel の対応:

| CollisionTypeIdDef | EObjectChannel     | 現在の用途 |
| ------------------ | ------------------ | ---------- |
| kDefault           | Default            | 未分類/既定 |
| kPlayer            | Player             | プレイヤー本体/プレイヤーHurtbox |
| kWeapon            | Weapon             | 武器用予約 |
| kEnemy             | Enemy              | 通常敵 |
| kBullet            | PlayerProjectile   | プレイヤー弾 |
| kEnemyBullet       | EnemyProjectile    | 敵弾 |
| kItem              | Item               | アイテム |
| kDummy             | Dummy              | ダミー |
| kBoss              | Boss               | ボス |
| kBossBullet        | BossProjectile     | ボス弾 |
| kWorld             | WorldStatic        | ステージ/ワールド |
| kTragetLock        | TargetLock         | ターゲットロック用予約 |
| kCrystal           | Crystal            | 敵スポーンクリスタル |
*/

inline constexpr uint32_t ToCollisionTypeId(EObjectChannel channel)
{
	return static_cast<uint32_t>(channel);
}

inline constexpr EObjectChannel ToObjectChannel(CollisionTypeIdDef typeId)
{
	return static_cast<EObjectChannel>(static_cast<uint32_t>(typeId));
}

inline constexpr EObjectChannel ToObjectChannel(uint32_t typeId)
{
	return static_cast<EObjectChannel>(typeId);
}

inline constexpr uint32_t ToTraceChannelId(ETraceChannel channel)
{
	return static_cast<uint32_t>(channel);
}
