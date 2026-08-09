#define NOMINMAX
#include "ItemVisualEffect.h"

#include "Item.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace
{
	// アイテムの種類に応じた演出名を取得
	const char* ToItemEffectName(ItemType type)
	{
		switch (type)
		{
		case ItemType::HealSmall: return "HealSmall";
		case ItemType::AmmoSmall: return "AmmoSmall";
		case ItemType::NextStageKey: return "NextStageKey";
		case ItemType::None:
		default: return "None";
		}
	}
}

/// -------------------------------------------------------------
///				アイテム演出の開始（待機演出の開始）
/// -------------------------------------------------------------
void ItemVisualEffect::StartIdle(const Item& item)
{
	// すでに待機演出が開始されている場合は何もしない
	if (item.GetType() == ItemType::None) return;

	IdleState& state = idleStates_[&item];			// 新しいアイテムに対してはデフォルト構築される
	state.type = item.GetType();					// アイテムの種類を設定
	state.emitTimer = 0.0f;							// タイマーをリセット
	state.emitterName = BuildIdleEmitterName(item); // エミッター名を構築
}

/// -------------------------------------------------------------
///					アイテム演出の停止
/// -------------------------------------------------------------
void ItemVisualEffect::StopIdle(const Item& item)
{
	// すでに待機演出が開始されていない場合は何もしない
	auto it = idleStates_.find(&item);
	if (it == idleStates_.end()) return;

	// エミッターを削除
	if (auto* manager = K4E::GpuParticleManager::GetInstance())
	{
		manager->RemoveEmitter(it->second.emitterName);
	}

	// 待機演出の状態を削除
	idleStates_.erase(it);
}

/// -------------------------------------------------------------
/// 		アイテム演出の更新（待機演出のタイマー処理）
/// -------------------------------------------------------------
void ItemVisualEffect::UpdateIdle(const Item& item)
{
	// すでに待機演出が開始されていない場合は何もしない
	if (!settings_.itemEffectEnabled || !settings_.idleEffectEnabled || settings_.idleParticleCount <= 0)
		return;

	// 待機演出の状態を取得
	auto it = idleStates_.find(&item);
	if (it == idleStates_.end())
	{
		// まだ待機演出が開始されていない場合は開始する
		StartIdle(item);

		// 再度検索して取得
		it = idleStates_.find(&item);
		if (it == idleStates_.end()) return;
	}

	// 待機演出のタイマーを更新
	float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();
	IdleState& state = it->second;
	state.emitTimer += deltaTime;

	// 設定された間隔で粒子を発生させる
	const float interval = std::max(0.05f, settings_.idleEmitInterval);

	// タイマーが間隔に達していない場合は何もしない
	if (state.emitTimer < interval) return;

	// タイマーが間隔に達した場合は粒子を発生させ、タイマーをリセット
	state.emitTimer = 0.0f;

	// 粒子の発生位置をアイテムの位置から少し上にずらす
	K4E::Vector3 emitPosition = item.GetPosition();
	emitPosition.y += 0.25f; // アイテムの上方にずらす

	// 常時演出は短い間隔の少量バーストにして、出しっぱなしでもGPU負荷が増えすぎないようにする。
	EmitSpriteBurst(state.emitterName, GetIdleParticleType(state.type), emitPosition, static_cast<uint32_t>(settings_.idleParticleCount), 0.16f);

	// 最後に再生した演出の名前を更新
	lastPlayedEffectName_ = std::string(ToItemEffectName(state.type)) + " 待機粒子";
}

/// -------------------------------------------------------------
/// 				アイテム取得時の演出再生
/// -------------------------------------------------------------
void ItemVisualEffect::PlayPickup(ItemType type, const K4E::Vector3& position)
{
	// 取得演出が無効化されている場合は何もしない
	if (!settings_.itemEffectEnabled || !settings_.pickupEffectEnabled || settings_.pickupParticleCount <= 0)
		return;

	// 取得演出の位置をアイテムの位置から少し上にずらす
	K4E::Vector3 burstPosition = position;
	burstPosition.y += 0.2f;

	// 取得演出のバーストとリングのエミッター名を構築
	const std::string burstName = BuildPickupEmitterName(type);

	// 取得演出のリングのエミッター名を構築
	const std::string ringName = BuildPickupRingEmitterName(type);

	// 取得演出のバーストの半径を設定（粒子速度に応じて調整）
	const float burstRadius = std::max(0.08f, settings_.pickupParticleSpeed * 0.035f);

	// 取得演出のバーストとリングを発生させる
	EmitSpriteBurst(burstName, GetPickupParticleType(type), burstPosition, static_cast<uint32_t>(settings_.pickupParticleCount), burstRadius);

	// 取得演出のリングを発生させる（リングは1つだけで十分）
	EmitSpriteBurst(ringName, K4E::GpuParticleType::Shockwave, burstPosition, 1, std::max(0.35f, settings_.pickupParticleSpeed * 0.12f));

	// 最後に再生した演出の名前を更新
	lastPlayedEffectName_ = std::string(ToItemEffectName(type)) + " 取得バースト";
}

/// -------------------------------------------------------------
/// 		アイテム演出のクリア（全ての待機演出を停止）
/// -------------------------------------------------------------
void ItemVisualEffect::Clear()
{
	// すべての待機演出を停止する
	if (auto* manager = K4E::GpuParticleManager::GetInstance())
	{
		// 待機演出の状態をすべて削除する
		for (const auto& [item, state] : idleStates_)
		{
			(void)item;
			manager->RemoveEmitter(state.emitterName);
		}
	}

	// 待機演出の状態をクリアする
	idleStates_.clear();

	// 最後に再生した演出の名前をリセット
	lastPlayedEffectName_ = "未再生";
}

/// -------------------------------------------------------------
///						ImGuiによる設定UIの描画
/// -------------------------------------------------------------
void ItemVisualEffect::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("アイテム演出");
	ImGui::Checkbox("アイテム演出有効", &settings_.itemEffectEnabled);
	ImGui::Checkbox("待機演出有効", &settings_.idleEffectEnabled);
	ImGui::Checkbox("取得演出有効", &settings_.pickupEffectEnabled);
	ImGui::DragInt("待機粒子数", &settings_.idleParticleCount, 1, 0, 32);
	ImGui::DragFloat("待機粒子発生間隔", &settings_.idleEmitInterval, 0.01f, 0.05f, 2.0f, "%.2f秒");
	ImGui::DragInt("取得時粒子数", &settings_.pickupParticleCount, 1, 0, 128);
	ImGui::DragFloat("取得時粒子速度", &settings_.pickupParticleSpeed, 0.1f, 0.1f, 20.0f, "%.2f");
	ImGui::DragFloat("アイテム浮遊高さ", &settings_.itemFloatHeight, 0.01f, 0.0f, 2.0f, "%.2f");
	ImGui::DragFloat("アイテム浮遊速度", &settings_.itemFloatSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::DragFloat("アイテム回転速度", &settings_.itemRotationSpeed, 0.05f, 0.0f, 10.0f, "%.2f");
	ImGui::ColorEdit4("Heal演出色", &settings_.healEffectColor.x);
	ImGui::ColorEdit4("Ammo演出色", &settings_.ammoEffectColor.x);
	ImGui::Text("最後に再生したItem演出: %s", lastPlayedEffectName_.c_str());
#else
	(void)this;
#endif
}

/// -------------------------------------------------------------
/// 			アイテムの種類に応じた演出色の取得
/// -------------------------------------------------------------
K4E::Vector4 ItemVisualEffect::GetEffectColor(ItemType type) const
{
	switch (type)
	{
	case ItemType::HealSmall:
		return settings_.healEffectColor;
	case ItemType::AmmoSmall:
		return settings_.ammoEffectColor;
	case ItemType::NextStageKey:
		return { 0.25f, 1.0f, 1.0f, 1.0f };
	case ItemType::None:
	default:
		return { 1.0f, 1.0f, 1.0f, 0.5f };
	}
}

/// -------------------------------------------------------------
/// 			アイテムの種類に応じた演出名を取得
/// -------------------------------------------------------------
std::string ItemVisualEffect::BuildIdleEmitterName(const Item& item) const
{
	std::ostringstream oss;

	// エミッター名を構築（アイテムの種類とアドレスを組み合わせて一意にする）
	oss << "ItemIdle_" << ToItemEffectName(item.GetType()) << "_" << reinterpret_cast<std::uintptr_t>(&item);

	// 最後に再生した演出の名前を更新
	return oss.str();
}

/// -------------------------------------------------------------
/// 			アイテムの種類に応じた取得演出名を取得
/// -------------------------------------------------------------
std::string ItemVisualEffect::BuildPickupEmitterName(ItemType type)
{
	// 取得演出のエミッター名を構築
	return std::string("ItemPickup_") + ToItemEffectName(type);
}

/// -------------------------------------------------------------
/// 			アイテムの種類に応じた取得リング演出名を取得
/// -------------------------------------------------------------
std::string ItemVisualEffect::BuildPickupRingEmitterName(ItemType type)
{
	// 取得リング演出のエミッター名を構築
	return std::string("ItemPickupRing_") + ToItemEffectName(type);
}

/// -------------------------------------------------------------
/// 		アイテムの種類に応じた待機演出の粒子タイプを取得
/// -------------------------------------------------------------
K4E::GpuParticleType ItemVisualEffect::GetIdleParticleType(ItemType type) const
{
	// 待機演出の粒子タイプをアイテムの種類に応じて返す
	switch (type)
	{
	case ItemType::HealSmall:
		return K4E::GpuParticleType::Heal;
	case ItemType::AmmoSmall:
		return K4E::GpuParticleType::Spark;
	case ItemType::NextStageKey:
		return K4E::GpuParticleType::Ambient;
	case ItemType::None:
	default:
		return K4E::GpuParticleType::Ambient;
	}
}

/// -------------------------------------------------------------
/// 		アイテムの種類に応じた取得演出の粒子タイプを取得
/// -------------------------------------------------------------
K4E::GpuParticleType ItemVisualEffect::GetPickupParticleType(ItemType type) const
{
	// 取得演出の粒子タイプをアイテムの種類に応じて返す
	switch (type)
	{
	case ItemType::HealSmall:
		return K4E::GpuParticleType::Heal;
	case ItemType::AmmoSmall:
		return K4E::GpuParticleType::Spark;
	case ItemType::NextStageKey:
		return K4E::GpuParticleType::DeathBurstCore;
	case ItemType::None:
	default:
		return K4E::GpuParticleType::Spark;
	}
}

/// -------------------------------------------------------------
/// エミッター名、粒子タイプ、位置、粒子数、半径を指定してスプライトバーストを発生させる
/// -------------------------------------------------------------
void ItemVisualEffect::EmitSpriteBurst(const std::string& emitterName, K4E::GpuParticleType particleType, const K4E::Vector3& position, uint32_t count, float radius)
{
	// 粒子数が0の場合は何もしない
	if (count == 0)	return;

	// GPUパーティクルマネージャーのインスタンスを取得
	auto* manager = K4E::GpuParticleManager::GetInstance();

	// マネージャーが存在しない場合は何もしない
	if (!manager) return;

	// エミッターを取得（存在しない場合は新規作成）
	K4E::GpuParticleEmitter* emitter = manager->GetEmitter(emitterName);

	// エミッターが存在しない場合は新規作成
	if (!emitter)
	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "Effects/white.dds";			 // 白色のテクスチャを使用
		info.radius = radius;								 // 半径
		info.loopCount = 0;									 // ループ回数
		info.loopFrequency = 0.0f;							 // ループ頻度
		info.drawType = 0;									 // 描画タイプ
		info.kind = K4E::GpuParticleKind::Sprite;			 // 粒子種類
		info.spriteType = particleType;						 // スプライトタイプ
		info.billboardFlags = K4E::BillboardMode::Camera;	 // ビルボードフラグ
		emitter = manager->CreateEmitter(emitterName, info); // エミッターを作成
	}

	// エミッターが存在しない場合は何もしない
	if (!emitter) return;

	emitter->GetInfoMutable().radius = radius; // 半径を更新
	emitter->SetPosition(position);			   // 位置を更新
	emitter->RequestEmit(count);			   // 粒子の発生をリクエスト
}
