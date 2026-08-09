#pragma once
#include <cstdint>

/// ---------- 識別IDの定義 ---------- ///
// 既存TypeIDは第1段階ではそのまま残し、EObjectChannelへの対応元として扱う。
enum class CollisionTypeIdDef : uint32_t
{
	kDefault,		 // デフォルトID 0
	kPlayer,		 // プレイヤーID 1
	kWeapon,		 // 武器ID 2
	kEnemy,			 // エネミーID 3
	kBullet,		 // 弾丸ID 4
	kEnemyBullet,	 // 敵弾ID 5
	kItem,			 // アイテムID 6
	kDummy,			 // ダミーID 7
	kBoss,			 // ボスID 8
	kBossBullet,	 // ボス弾ID 9
	kWorld,			 // ワールドID 10
	kTragetLock,	 // ターゲットロック用ID 11
	kCrystal,		 // 敵スポーンクリスタルID 12
};
