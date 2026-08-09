#pragma once

namespace Ken4lowEngine
{
// エフェクトの種類を列挙型で定義
enum class ParticleEffectType
{
	Default, 	  // デフォルト
	Slash,		  // スラッシュ
	Ring,		  // リング
	Blast,		  // 爆風
	Cylinder,	  // シリンダー
	Star,		  // 星型
	Smoke,		  // 煙
	Flash,		  // フラッシュ
	Spark,		  // 火花
	Debris,		  // 破片
	EnergyGather, // 収束
	Charge,       // 収束中にぐるぐる回る
	Explosion,    // 爆発用
	Blood,		  // 血飛沫
	LaserBeam,	  // レーザービーム
};
} // namespace Ken4lowEngine
