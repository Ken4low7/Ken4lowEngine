#pragma once
#include "ItemType.h"
#include "Vector3.h"
#include "Vector4.h"
#include "GpuParticleType.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 --------- ///
class Item;

/// -------------------------------------------------------------
///						アイテム演出管理クラス
/// -------------------------------------------------------------
class ItemVisualEffect
{
private: /// ---------- 構造体 ---------- ///

	// アイテム演出の設定値を保持する構造体
	struct Settings
	{
		bool itemEffectEnabled = true;	  // アイテム演出全体の有効/無効
		bool idleEffectEnabled = true;	  // 待機演出の有効/無効
		bool pickupEffectEnabled = true;  // 取得演出の有効/無効
		int idleParticleCount = 2;		  // 待機演出の粒子数
		float idleEmitInterval = 0.28f;	  // 待機演出の粒子発生間隔（秒）
		int pickupParticleCount = 18;	  // 取得演出の粒子数
		float pickupParticleSpeed = 4.5f; // 取得演出の粒子速度
		float itemFloatHeight = 0.18f;	  // アイテムの浮遊高さ
		float itemFloatSpeed = 2.2f;	  // アイテムの浮遊速度
		float itemRotationSpeed = 1.6f;	  // アイテムの回転速度

		// アイテム演出の色設定（RGBA）
		K4E::Vector4 healEffectColor = { 0.25f, 1.0f, 0.45f, 1.0f };

		// Ammo演出の色設定（RGBA）
		K4E::Vector4 ammoEffectColor = { 1.0f, 0.75f, 0.15f, 1.0f };
	};

	// 待機演出の状態を保持する構造体
	struct IdleState
	{
		ItemType type = ItemType::None; // アイテムの種類
		float emitTimer = 0.0f;			// 待機演出の粒子発生タイマー
		std::string emitterName;		// 待機演出のエミッター名
	};

public: /// ---------- メンバ関数 ---------- ///

	// アイテム演出の開始（待機演出の開始）
	void StartIdle(const Item& item);

	// アイテム演出の停止
	void StopIdle(const Item& item);

	// アイテム演出の更新（待機演出のタイマー処理）
	void UpdateIdle(const Item& item);

	// アイテム取得時の演出再生
	void PlayPickup(ItemType type, const K4E::Vector3& position);

	// アイテム演出のクリア（全ての待機演出を停止）
	void Clear();

	// ImGuiによる設定UIの描画
	void DrawImGui();

public: /// ---------- ゲッター ---------- ///

	// 設定値の取得
	Settings& GetSettings() { return settings_; }

	// 設定値の取得（const版）
	const Settings& GetSettings() const { return settings_; }

	// アイテムの種類に応じた演出色の取得
	K4E::Vector4 GetEffectColor(ItemType type) const;

	// 最後に再生した演出の名前を取得
	const std::string& GetLastPlayedEffectName() const { return lastPlayedEffectName_; }

private: /// ---------- メンバ関数 ---------- ///

	// アイテムの種類に応じた演出名を取得
	std::string BuildIdleEmitterName(const Item& item) const;

	// アイテムの種類に応じた取得演出名を取得
	std::string BuildPickupEmitterName(ItemType type);

	// アイテムの種類に応じた取得リング演出名を取得
	std::string BuildPickupRingEmitterName(ItemType type);

	// アイテムの種類に応じた待機演出の粒子タイプを取得
	K4E::GpuParticleType GetIdleParticleType(ItemType type) const;

	// アイテムの種類に応じた取得演出の粒子タイプを取得
	K4E::GpuParticleType GetPickupParticleType(ItemType type) const;

	// エミッター名、粒子タイプ、位置、粒子数、半径を指定してスプライトバーストを発生させる
	void EmitSpriteBurst(const std::string& emitterName, K4E::GpuParticleType particleType, const K4E::Vector3& position, uint32_t count, float radius);

private: /// ---------- メンバ変数 ---------- ///

	// 待機演出の設定値を保持する構造体
	Settings settings_{};

	// 待機演出の状態を保持するマップ（アイテムポインタをキーにしてIdleStateを保持）
	std::unordered_map<const Item*, IdleState> idleStates_;

	// 最後に再生した演出の名前を保持する文字列
	std::string lastPlayedEffectName_ = "未再生";
};
