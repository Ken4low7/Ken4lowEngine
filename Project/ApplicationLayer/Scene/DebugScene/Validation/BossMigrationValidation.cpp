#include "BossMigrationValidation.h"

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActorAttackComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossBrainComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossWeakPointComponent.h"

#include <ActorWorld.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	const K4E::Vector3 kBossPosition{ 0.0f, 3.0f, 18.0f };
}

void BossMigrationValidation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	boss_ = &actorWorld_->SpawnActor<K4E::BossActor>();
	const bool loadedFromPrefab = actorWorld_->ReloadActorFromJson(*boss_, jsonPath_);
	if (loadedFromPrefab)
	{
		boss_->Initialize();
		lastMessage_ = "BossActorを保存済みPrefabから自動読込しました。";
		lastSucceeded_ = true;
	}
	else
	{
		boss_->ResetForValidation(kBossPosition);
		lastMessage_ = "Boss Prefabが無いためコード既定値で生成しました。";
		lastSucceeded_ = false;
	}
	boss_->SetName(bossName_);
	boss_->SetLayer("BossValidation");
	boss_->AddTag("Boss");
	RefreshActorReferencesAndBindings();
}

void BossMigrationValidation::Update(){ RefreshActorReferencesAndBindings(); ProcessRequests(); }
void BossMigrationValidation::UpdateEditor(){ RefreshActorReferencesAndBindings(); ProcessRequests(); }

void BossMigrationValidation::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("ボス Player Target 検証"))
	{
		ImGui::End();
		return;
	}
	const auto* health = boss_ ? boss_->GetHealthComponent() : nullptr;
	const auto* movement = boss_ ? boss_->GetMovementComponent() : nullptr;
	const auto* collider = boss_ ? boss_->GetColliderComponent() : nullptr;
	const auto* animation = boss_ ? boss_->GetAnimationComponent() : nullptr;
	const auto* visual = boss_ ? boss_->GetHumanoidVisualComponent() : nullptr;
	const auto* brain = boss_ ? boss_->GetBossBrainComponent() : nullptr;
	const auto* attack = boss_ ? boss_->GetBossAttackComponent() : nullptr;
	const auto* phase = boss_ ? boss_->GetBossPhaseComponent() : nullptr;
	const auto* weakPoint = boss_ ? boss_->GetBossWeakPointComponent() : nullptr;
	const auto* presentation = boss_ ? boss_->GetBossPresentationComponent() : nullptr;
	const auto* rigidbodyComponent = boss_ ? boss_->GetComponent<K4E::RigidbodyComponent>() : nullptr;
	const auto* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
	const bool structureReady = boss_ && health && movement && collider && animation && visual && brain && attack && phase && weakPoint && presentation && rigidbody;

	ImGui::Text("Target: %s", target_ ? target_->GetName().c_str() : "なし");
	ImGui::Text("BossActor構成: %s", structureReady ? "OK" : "不足");
	ImGui::Text("HP: %.0f / %.0f / Phase %d", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f, phase ? phase->GetCurrentPhase() : 0);
	ImGui::Text("Brain: %s / Attack: %s", brain ? brain->GetStateName().c_str() : "None", attack ? attack->GetLastSelectedAttackId().c_str() : "None");
	ImGui::Text("Animation: %s / Presentation: %s", animation ? animation->GetAnimationName().c_str() : "None", presentation ? presentation->GetStateName().c_str() : "None");
	ImGui::Text("Visual: %zu parts / Shadow %s", visual ? visual->GetParts().size() : 0u, visual && visual->IsCastShadowEnabled() ? "ON" : "OFF");
	ImGui::Text("WeakPoint: %s / Head x%.2f", weakPoint && weakPoint->HasValidPartReferences() ? "OK" : "未解決", weakPoint ? weakPoint->ResolveDamageMultiplier("Head") : 0.0f);
	ImGui::Text("Collider: %s / Movement: %s / Physics: %s", collider && collider->IsActive() ? "有効" : "停止", movement && movement->IsMovementEnabled() ? "有効" : "停止", rigidbody ? "Dynamic" : "None");
	if (boss_)
	{
		const K4E::Vector3 deathPosition = boss_->GetDeathWorldPosition();
		ImGui::Text("Death position: %.2f %.2f %.2f / Presentation: %s", deathPosition.x, deathPosition.y, deathPosition.z, boss_->IsDeathPresentationComplete() ? "complete" : "running");
	}
	ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", lastMessage_.c_str());

	if (ImGui::Button("Headへ100基礎Damage")) requestHeadDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Bodyへ100基礎Damage")) requestBodyDamage_ = true;
	if (ImGui::Button("撃破地点固定テスト")) requestDeathPositionTest_ = true;
	if (ImGui::Button("全フェーズ攻撃選択を検証")) requestSelectionRuleValidation_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Boss Reset")) requestReset_ = true;
	if (ImGui::Button("Boss JSON保存")) requestSave_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Boss JSON再読込")) requestReload_ = true;
	ImGui::TextDisabled("全フェーズ検証はPhase 1〜3とNear/Middle/Farの全候補登録を確認します。");
	ImGui::End();
#endif
}

void BossMigrationValidation::Finalize()
{
	boss_ = nullptr;
	target_ = nullptr;
	actorWorld_ = nullptr;
}

void BossMigrationValidation::RefreshActorReferencesAndBindings()
{
	if (!actorWorld_)
	{
		boss_ = nullptr;
		target_ = nullptr;
		return;
	}
	boss_ = dynamic_cast<K4E::BossActor*>(actorWorld_->FindActorByName(bossName_));
	target_ = dynamic_cast<K4E::CharacterActor*>(actorWorld_->FindActorByName(targetName_));
	if (!boss_)
	{
		lastSucceeded_ = false;
		lastMessage_ = "BossActorが見つかりません。";
		return;
	}
	boss_->SetTargetActor(target_);
	if (!boss_->IsDead())
	{
		lastSucceeded_ = target_ != nullptr;
		lastMessage_ = lastSucceeded_ ? "BossActorはDebugPlayerをTargetとして追跡・攻撃しています。" : "DebugPlayerが見つかりません。";
	}
}

void BossMigrationValidation::ProcessRequests()
{
	if (requestHeadDamage_)
	{
		requestHeadDamage_ = false;
		const K4E::CharacterDamageResult result = boss_ ? boss_->ApplyWeakPointDamage("Head", 100.0f) : K4E::CharacterDamageResult{};
		lastSucceeded_ = result.accepted;
		lastMessage_ = result.accepted ? "Head弱点へ倍率適用ダメージを与えました。" : "Head弱点ダメージを適用できませんでした。";
	}
	if (requestBodyDamage_)
	{
		requestBodyDamage_ = false;
		const K4E::CharacterDamageResult result = boss_ ? boss_->ApplyWeakPointDamage("Body", 100.0f) : K4E::CharacterDamageResult{};
		lastSucceeded_ = result.accepted;
		lastMessage_ = result.accepted ? "Bodyへ通常倍率ダメージを与えました。" : "Bodyダメージを適用できませんでした。";
	}
	if (requestDeathPositionTest_)
	{
		requestDeathPositionTest_ = false;
		if (boss_)
		{
			boss_->ResetForValidation(kBossPosition);
			const K4E::Vector3 before = boss_->GetPosition();
			const K4E::CharacterDamageResult result = boss_->ApplyDamage(boss_->GetHP() + 1.0f);
			const K4E::Vector3 after = boss_->GetPosition();
			lastSucceeded_ = result.killed && K4E::Vector3::LengthSquared(after - before) <= 0.0001f;
			lastMessage_ = lastSucceeded_ ? "Bossは撃破地点を維持したまま死亡演出へ移行しました。" : "Bossの撃破地点固定に失敗しました。";
		}
	}
	if (requestSelectionRuleValidation_)
	{
		requestSelectionRuleValidation_ = false;
		const K4E::BossAttackComponent* attack = boss_ ? boss_->GetBossAttackComponent() : nullptr;
		std::string validationSummary;
		lastSucceeded_ = attack && attack->ValidateSelectionRules(validationSummary);
		lastMessage_ = attack
			? (lastSucceeded_ ? "攻撃選択ルール検証OK: " : "攻撃選択ルール検証NG: ") + validationSummary
			: "BossAttackComponentが見つからないため検証できませんでした。"; // DebugScene上で全フェーズ・全距離帯の登録漏れを即座に確認する。
	}
	if (requestReset_)
	{
		requestReset_ = false;
		if (boss_) boss_->ResetForValidation(kBossPosition);
		RefreshActorReferencesAndBindings();
	}
	if (requestSave_)
	{
		requestSave_ = false;
		lastSucceeded_ = actorWorld_ && boss_ && actorWorld_->SaveActorToJson(*boss_, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "BossActor全体をPrefabへ保存しました。" : "BossActor Prefab保存に失敗しました。";
	}
	if (requestReload_)
	{
		requestReload_ = false;
		lastSucceeded_ = actorWorld_ && boss_ && actorWorld_->ReloadActorFromJson(*boss_, jsonPath_);
		if (lastSucceeded_ && boss_) boss_->Initialize();
		lastMessage_ = lastSucceeded_ ? "BossActorをPrefabから復元しました。" : "BossActor Prefab復元に失敗しました。";
		RefreshActorReferencesAndBindings();
	}
}
