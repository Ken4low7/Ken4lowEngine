#pragma once
#include <vector>
#include <algorithm>
#include "Vector3.h"
#include "EnemyType.h"

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
class CharacterWorld;

/// ---------- 1体分のスポーン情報 ---------- ///
struct WaveSpawnEntry
{
	K4E::Vector3 position = { 0.0f, 0.0f, 0.0f }; // スポーン位置
	EnemyType enemyType = EnemyType::Melee; // 未指定の既存ステージは最新の近接雑魚敵へ寄せる。
};

/// ---------- 1ウェーブ分の定義 ---------- ///
struct WaveDefinition
{
	std::vector<WaveSpawnEntry> enemies; // このウェーブに含まれるスポーン情報のリスト
	float delayBeforeSpawnSec = 0.0f; // ウェーブ開始前の待機時間（秒）
};

/// -------------------------------------------------------------
///				　	　ウェーブ管理クラス
///
/// GamePlayWorldが所有し、WaveDefense系ステージで敵スポーンの進行を管理する。
/// 敵の実体はCharacterWorldへ生成し、このクラスは「いつ、どのWaveを開始したか」の状態だけを持つ。
/// -------------------------------------------------------------
class WaveManager
{
public: /// ---------- メンバ関数 ---------- ///

	// 設定済みWaveは保持したまま、開始状態・現在Wave・待機タイマーを初期状態へ戻す。
	void Reset();

	/// <summary>
	/// ウェーブを設定します。
	/// </summary>
	/// <param name="waves">設定するウェーブ定義のベクター。</param>
	void SetWaves(const std::vector<WaveDefinition>& waves);

	/// <summary>
	/// Wave進行を開始します。以後Updateで待機時間を消化して敵をスポーンします。
	/// </summary>
	void Start();

	/// <summary>
	/// Wave待機タイマーと敵残数を見て、必要なら次WaveをCharacterWorldへスポーンします。
	/// </summary>
	/// <param name="characters">更新するキャラクターワールドへの参照。</param>
	/// <param name="deltaTime">前回の更新からの経過時間(秒)。</param>
	void Update(CharacterWorld& characters, float deltaTime);

public: /// ---------- アクセッサ ---------- ///

	bool HasStarted() const { return started_; } // ウェーブスポーンが開始されたかどうか
	bool IsWaveInProgress() const { return waveInProgress_; } // 現在ウェーブがスポーン中かどうか
	bool IsWaitingNextWave() const { return started_ && !waveInProgress_ && !allWavesCleared_; } // 次のウェーブスポーン待ちかどうか
	bool IsAllWavesCleared() const { return allWavesCleared_; } // すべてのウェーブがクリアされたかどうか

	int GetCurrentWaveIndex() const { return currentWaveIndex_; } // 現在のウェーブのインデックス
	int GetCurrentWaveNumber() const { return currentWaveIndex_ + 1; } // 現在のウェーブの番号（1始まり）
	int GetTotalWaveCount() const { return static_cast<int>(waves_.size()); } // 総ウェーブ数

	float GetNextWaveTimer() const { return nextWaveTimer_; } // 次のウェーブスポーンまでのタイマー（秒）

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// ウェーブをスポーンします。
	/// </summary>
	/// <param name="characters">キャラクターワールド。</param>
	/// <param name="wave">スポーンするウェーブの定義。</param>
	int SpawnWave(CharacterWorld& characters, const WaveDefinition& wave);

private: /// ---------- メンバ変数 ---------- ///

	std::vector<WaveDefinition> waves_; // ウェーブ定義のリスト

	int currentWaveIndex_ = 0; // 現在のウェーブのインデックス
	float nextWaveTimer_ = 0.0f; // 次のウェーブスポーンまでのタイマー（秒）

	bool started_ = false; // ウェーブスポーンが開始されたかどうか
	bool waveInProgress_ = false; // 現在ウェーブがスポーン中かどうか
	bool allWavesCleared_ = false; // すべてのウェーブがクリアされたかどうか
	int currentWaveSpawnedCount_ = 0; // 現在のウェーブでスポーンした敵の数
};

