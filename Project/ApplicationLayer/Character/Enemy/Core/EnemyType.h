#pragma once

#include <string_view>

/// 通常ゲームで生成する雑魚敵の種類。StressTest専用のScalableEnemyTypeとは分離して扱う。
enum class EnemyType
{
	Melee,
	MidRange,
};

inline EnemyType ParseEnemyType(std::string_view enemyType)
{
	if (enemyType == "Melee" || enemyType == "melee")
	{
		return EnemyType::Melee;
	}
	if (enemyType == "MidRange" || enemyType == "midRange" || enemyType == "midrange")
	{
		return EnemyType::MidRange;
	}
	// 旧Legacy・空文字・未知名は既存ステージ互換を保ちながら最新の近接敵へ安全に寄せる。
	return EnemyType::Melee;
}
