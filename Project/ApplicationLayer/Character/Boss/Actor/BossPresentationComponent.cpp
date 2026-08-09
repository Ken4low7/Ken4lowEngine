#define NOMINMAX
#include "BossPresentationComponent.h"

#include "BossActor.h"
#include "BossActorAttackComponent.h"
#include "ApplicationLayer/Character/Boss/Attacks/BossAttackEffects.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"

#include <AudioManager.h>
#include <Camera.h>
#include <GpuParticleType.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <TextComponent.h>
#include <Vector4.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iterator>
#include <numbers>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
		constexpr std::size_t kAfterimagePoolSize = 8u;
		constexpr float kAfterimageLifetime = 0.34f;
		constexpr float kAfterimageSpawnInterval = 0.055f;
		constexpr float kChargeGuideInterval = 0.065f;
		constexpr float kChargeGuideStartDistance = 1.6f;
		constexpr float kChargeGuideSpacing = 1.75f;

		float Clamp01(float value)
		{
			return std::clamp(value, 0.0f, 1.0f);
		}

		Vector4 LerpColor(const Vector4& from, const Vector4& to, float amount)
		{
			const float t = Clamp01(amount);
			return {
				from.x + (to.x - from.x) * t,
				from.y + (to.y - from.y) * t,
				from.z + (to.z - from.z) * t,
				from.w + (to.w - from.w) * t
			};
		}

		bool ContainsAttackToken(std::string_view attackId, std::string_view token)
		{
			return attackId.find(token) != std::string_view::npos;
		}

		bool IsChargeAttack(std::string_view attackId)
		{
			return ContainsAttackToken(attackId, "Charge");
		}

		bool IsGroundSlamAttack(std::string_view attackId)
		{
			return ContainsAttackToken(attackId, "GroundSlam");
		}

		bool IsShockwaveAttack(std::string_view attackId)
		{
			return ContainsAttackToken(attackId, "Shockwave");
		}

		bool IsAreaAttack(std::string_view attackId)
		{
			return IsGroundSlamAttack(attackId) || IsShockwaveAttack(attackId);
		}

		Vector3 NormalizeDirectionXZ(const Vector3& direction)
		{
			const float length = Vector3::LengthXZ(direction);
			return length > 0.0001f ? Vector3{ direction.x / length, 0.0f, direction.z / length } : Vector3{ 0.0f, 0.0f, 1.0f };
		}
	}

	void BossPresentationComponent::Initialize()
	{
		Actor* owner = GetOwner();
		const auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		observedPhaseRevision_ = phase ? phase->GetPhaseRevision() : 0;
		presentedPhase_ = phase ? phase->GetCurrentPhase() : 1;
		phaseTransitionDuration_ = std::isfinite(phaseTransitionDuration_) ? std::max(0.01f, phaseTransitionDuration_) : 0.5f;
		deathPresentationDuration_ = std::isfinite(deathPresentationDuration_) ? std::max(0.01f, deathPresentationDuration_) : 1.2f;
		telegraphParticleInterval_ = std::isfinite(telegraphParticleInterval_) ? std::max(0.03f, telegraphParticleInterval_) : 0.10f;
		phaseAuraInterval_ = std::isfinite(phaseAuraInterval_) ? std::max(0.05f, phaseAuraInterval_) : 0.18f;
		SetDrawOrder(-10); // 残像を本体より先に描き、本体の輪郭を最後に明瞭に重ねる。
		EnsureAttackListener();
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::Update(float deltaTime)
	{
		EnsureAttackListener();
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f)
		{
			ApplyVisualAppearance();
			return;
		}

		visualTime_ += deltaTime;
		UpdateAfterimages(deltaTime);

		Actor* owner = GetOwner();
		auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		if (!deathPresentationActive_ && phase && phase->GetPhaseRevision() != observedPhaseRevision_)
		{
			observedPhaseRevision_ = phase->GetPhaseRevision();
			StartPhaseTransition(phase->GetCurrentPhase());
		}

		if (attackTelegraphActive_) UpdateAttackTelegraph(deltaTime);
		if (chargeTrailActive_) UpdateChargeTrail(deltaTime);

		if (!deathPresentationActive_ && presentedPhase_ >= 2)
		{
			phaseParticleTimer_ += deltaTime;
			const float interval = phaseTransitionActive_ ? 0.07f : (presentedPhase_ >= 3 ? phaseAuraInterval_ : phaseAuraInterval_ * 1.45f);
			if (phaseParticleTimer_ >= interval)
			{
				phaseParticleTimer_ = std::fmod(phaseParticleTimer_, interval);
				EmitPhasePulse(false);
			}
		}

		if (phaseTransitionActive_ || deathPresentationActive_)
		{
			elapsed_ += deltaTime;
			const float duration = deathPresentationActive_ ? deathPresentationDuration_ : phaseTransitionDuration_;
			if (phaseTransitionActive_ && elapsed_ >= duration)
			{
				phaseTransitionActive_ = false;
				stateName_ = attackTelegraphActive_ ? "Telegraph: " + activeAttackId_ : "Idle";
				if (auto* character = dynamic_cast<CharacterActor*>(owner))
				{
					if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Idle", 1.5f, true);
				}
				if (auto* boss = dynamic_cast<BossActor*>(owner))
				{
					if (TextComponent* label = boss->GetHealthLabelComponent()) label->SetText("BOSS HP");
				}
			}
		}

		UpdateCameraShake(deltaTime);
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::Draw()
	{
		std::vector<const AfterimageSnapshot*> activeSnapshots;
		activeSnapshots.reserve(afterimagePool_.size());
		for (const AfterimageSnapshot& snapshot : afterimagePool_)
		{
			if (snapshot.active) activeSnapshots.push_back(&snapshot);
		}
		std::sort(activeSnapshots.begin(), activeSnapshots.end(), [](const AfterimageSnapshot* left, const AfterimageSnapshot* right)
			{
				return left->age > right->age; // 古い残像から描き、最も新しい残像を本体の直後へ重ねる。
			});
		for (const AfterimageSnapshot* snapshot : activeSnapshots)
		{
			for (const AfterimagePart& part : snapshot->parts)
			{
				if (part.visible && part.object) part.object->Draw();
			}
		}
	}

	void BossPresentationComponent::Finalize()
	{
		Actor* owner = GetOwner();
		auto* currentAttack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr;
		if (currentAttack && currentAttack == boundAttack_ && attackListenerId_ != 0) currentAttack->RemoveAttackListener(attackListenerId_);
		boundAttack_ = nullptr;
		telegraphTarget_ = nullptr;
		attackListenerId_ = 0;
		activeAttackId_.clear();
		attackTelegraphActive_ = false;
		StopChargeTrail();
		afterimagePool_.clear();
		nextAfterimageIndex_ = 0;
		cameraShakeTimer_ = cameraShakeDuration_ = cameraShakeAmplitude_ = cameraShakeFrequency_ = cameraShakeSeed_ = 0.0f;
	}

	void BossPresentationComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		int activeAfterimageCount = 0;
		for (const AfterimageSnapshot& snapshot : afterimagePool_) if (snapshot.active) ++activeAfterimageCount;
		ImGui::SeparatorText("ボス演出");
		ImGui::Text("状態: %s / Phase: %d / 経過: %.2f", stateName_.c_str(), presentedPhase_, elapsed_);
		ImGui::Text("予兆: %s / %.2f / %.2f", attackTelegraphActive_ ? activeAttackId_.c_str() : "None", telegraphElapsed_, telegraphDuration_);
		ImGui::Text("突進Trail: %s / %.2f / %.2f", chargeTrailActive_ ? "Active" : "Idle", chargeTrailElapsed_, chargeTrailDuration_);
		ImGui::Text("残像: %d / %zu", activeAfterimageCount, afterimagePool_.size());
		ImGui::Text("攻撃Shake: %.2f / %.2f", cameraShakeTimer_, cameraShakeAmplitude_);
#endif
	}

	void BossPresentationComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["PhaseTransitionDuration"] = phaseTransitionDuration_;
		outJson["DeathPresentationDuration"] = deathPresentationDuration_;
		outJson["TelegraphParticleInterval"] = telegraphParticleInterval_;
		outJson["PhaseAuraInterval"] = phaseAuraInterval_;
	}

	void BossPresentationComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		phaseTransitionDuration_ = inJson.value("PhaseTransitionDuration", phaseTransitionDuration_);
		deathPresentationDuration_ = inJson.value("DeathPresentationDuration", deathPresentationDuration_);
		telegraphParticleInterval_ = inJson.value("TelegraphParticleInterval", telegraphParticleInterval_);
		phaseAuraInterval_ = inJson.value("PhaseAuraInterval", phaseAuraInterval_);
		phaseTransitionDuration_ = std::isfinite(phaseTransitionDuration_) ? std::max(0.01f, phaseTransitionDuration_) : 0.5f;
		deathPresentationDuration_ = std::isfinite(deathPresentationDuration_) ? std::max(0.01f, deathPresentationDuration_) : 1.2f;
		telegraphParticleInterval_ = std::isfinite(telegraphParticleInterval_) ? std::max(0.03f, telegraphParticleInterval_) : 0.10f;
		phaseAuraInterval_ = std::isfinite(phaseAuraInterval_) ? std::max(0.05f, phaseAuraInterval_) : 0.18f;
	}

	void BossPresentationComponent::StartDeathPresentation()
	{
		FinishAttackTelegraph();
		ResetAfterimages();
		phaseTransitionActive_ = false;
		deathPresentationActive_ = true;
		elapsed_ = 0.0f;
		stateName_ = "Death";
		Actor* owner = GetOwner();
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->SetAttackEnabled(false);
		if (auto* character = dynamic_cast<CharacterActor*>(owner))
		{
			if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Boss.Dead", deathPresentationDuration_, false);
		}
		StartCameraShake(0.65f, 0.34f, 22.0f);
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::ResetPresentation()
	{
		phaseTransitionActive_ = false;
		deathPresentationActive_ = false;
		elapsed_ = 0.0f;
		visualTime_ = 0.0f;
		phaseParticleTimer_ = 0.0f;
		FinishAttackTelegraph();
		ResetAfterimages();
		stateName_ = "Idle";
		cameraShakeTimer_ = cameraShakeDuration_ = cameraShakeAmplitude_ = cameraShakeFrequency_ = cameraShakeSeed_ = 0.0f;
		Actor* owner = GetOwner();
		const auto* phase = owner ? owner->GetComponent<BossPhaseComponent>() : nullptr;
		observedPhaseRevision_ = phase ? phase->GetPhaseRevision() : 0;
		presentedPhase_ = phase ? phase->GetCurrentPhase() : 1;
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->SetAttackEnabled(true);
		if (auto* boss = dynamic_cast<BossActor*>(owner))
		{
			if (TextComponent* label = boss->GetHealthLabelComponent()) label->SetText("BOSS HP");
		}
		EnsureAttackListener();
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::EnsureAttackListener()
	{
		Actor* owner = GetOwner();
		auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr;
		if (attack == boundAttack_ && attackListenerId_ != 0) return;
		boundAttack_ = attack;
		attackListenerId_ = 0;
		if (!boundAttack_) return;
		attackListenerId_ = boundAttack_->AddAttackListener([this](const AttackEvent& event) { HandleAttackEvent(event); }); // JSON再読込後も新しいAttack実体から予兆イベントを受け取る。
	}

	void BossPresentationComponent::HandleAttackEvent(const AttackEvent& event)
	{
		switch (event.type)
		{
		case AttackEventType::Started:
			StartAttackTelegraph(event);
			break;
		case AttackEventType::Executed:
			EmitAttackTelegraphPulse(true);
			if (IsGroundSlamAttack(event.attackId)) StartCameraShake(0.38f, 0.30f, 25.0f);
			else if (IsShockwaveAttack(event.attackId)) StartCameraShake(0.26f, ContainsAttackToken(event.attackId, "Fast") ? 0.20f : 0.17f, 24.0f);
			else if (IsChargeAttack(event.attackId) && event.result.accepted) StartCameraShake(0.18f, 0.13f, 28.0f);
			if (IsAreaAttack(event.attackId)) AudioManager::GetInstance()->PlaySE("enemy_death.mp3", 0.18f, ContainsAttackToken(event.attackId, "Fast") ? 1.08f : 0.84f);
			else if (event.result.accepted) AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.14f, IsChargeAttack(event.attackId) ? 0.72f : 1.05f);
			break;
		case AttackEventType::Ended:
		case AttackEventType::Interrupted:
			FinishAttackTelegraph();
			break;
		default:
			break;
		}
	}

	void BossPresentationComponent::StartAttackTelegraph(const AttackEvent& event)
	{
		if (deathPresentationActive_ || phaseTransitionActive_) return;
		StopChargeTrail();
		activeAttackId_ = event.attackId;
		telegraphTarget_ = event.target;
		telegraphElapsed_ = 0.0f;
		telegraphParticleTimer_ = telegraphParticleInterval_;
		chargeDirectionLocked_ = false;
		const AttackData* attackData = boundAttack_ ? boundAttack_->FindAttackData(activeAttackId_) : nullptr;
		telegraphDuration_ = attackData ? std::max(0.05f, attackData->windupTime) : 0.15f;
		attackTelegraphActive_ = true;
		stateName_ = "Telegraph: " + activeAttackId_;

		const std::string_view attackId(activeAttackId_);
		const float pitch = IsChargeAttack(attackId) ? (ContainsAttackToken(attackId, "Frenzy") ? 0.56f : 0.68f) : (IsAreaAttack(attackId) ? 0.50f : 0.92f);
		AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", IsAreaAttack(attackId) ? 0.14f : 0.10f, pitch);
		EmitAttackTelegraphPulse(false);
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::UpdateAttackTelegraph(float deltaTime)
	{
		telegraphElapsed_ += deltaTime;
		telegraphParticleTimer_ += deltaTime;
		if (telegraphElapsed_ >= telegraphDuration_)
		{
			attackTelegraphActive_ = false;
			telegraphElapsed_ = telegraphDuration_;
			telegraphParticleTimer_ = 0.0f;
			if (IsChargeAttack(activeAttackId_))
			{
				const AttackData* attackData = boundAttack_ ? boundAttack_->FindAttackData(activeAttackId_) : nullptr;
				BeginChargeTrail(attackData ? attackData->activeTime : 0.5f);
			}
			if (!phaseTransitionActive_ && !deathPresentationActive_) stateName_ = "Attack: " + activeAttackId_;
			return;
		}
		if (telegraphParticleTimer_ >= telegraphParticleInterval_)
		{
			telegraphParticleTimer_ = std::fmod(telegraphParticleTimer_, telegraphParticleInterval_);
			EmitAttackTelegraphPulse(false);
		}
	}

	void BossPresentationComponent::FinishAttackTelegraph()
	{
		attackTelegraphActive_ = false;
		StopChargeTrail();
		telegraphTarget_ = nullptr;
		telegraphElapsed_ = 0.0f;
		telegraphDuration_ = 0.0f;
		telegraphParticleTimer_ = 0.0f;
		chargeDirectionLocked_ = false;
		activeAttackId_.clear();
		if (!phaseTransitionActive_ && !deathPresentationActive_) stateName_ = "Idle";
	}

	Vector3 BossPresentationComponent::ResolveCurrentTargetDirection() const
	{
		const auto* boss = dynamic_cast<const BossActor*>(GetOwner());
		if (!boss) return { 0.0f, 0.0f, 1.0f };
		const Vector3 bossPosition = boss->GetPosition();
		if (telegraphTarget_)
		{
			if (const SceneComponent* targetRoot = telegraphTarget_->GetRootComponent())
			{
				return NormalizeDirectionXZ(targetRoot->GetWorldPosition() - bossPosition);
			}
		}
		return chargeDirectionLocked_ ? chargeDirection_ : Vector3{ 0.0f, 0.0f, 1.0f };
	}

	void BossPresentationComponent::BeginChargeTrail(float activeDuration)
	{
		chargeDirection_ = ResolveCurrentTargetDirection();
		chargeDirectionLocked_ = true;
		chargeTrailDuration_ = std::max(0.01f, activeDuration);
		chargeTrailElapsed_ = 0.0f;
		chargeGuideTimer_ = kChargeGuideInterval;
		afterimageSpawnTimer_ = kAfterimageSpawnInterval;
		chargeTrailActive_ = true;
		EnsureAfterimagePool();
		SpawnChargeAfterimage();
		EmitAttackTelegraphPulse(false); // Active開始地点でも固定方向の大型ガイドを即座に再表示する。
	}

	void BossPresentationComponent::UpdateChargeTrail(float deltaTime)
	{
		chargeTrailElapsed_ += deltaTime;
		chargeGuideTimer_ += deltaTime;
		afterimageSpawnTimer_ += deltaTime;

		while (chargeGuideTimer_ >= kChargeGuideInterval)
		{
			chargeGuideTimer_ -= kChargeGuideInterval;
			EmitAttackTelegraphPulse(false);
		}
		while (afterimageSpawnTimer_ >= kAfterimageSpawnInterval)
		{
			afterimageSpawnTimer_ -= kAfterimageSpawnInterval;
			SpawnChargeAfterimage();
		}
		if (chargeTrailElapsed_ >= chargeTrailDuration_) StopChargeTrail();
	}

	void BossPresentationComponent::StopChargeTrail()
	{
		chargeTrailActive_ = false;
		chargeTrailElapsed_ = 0.0f;
		chargeTrailDuration_ = 0.0f;
		chargeGuideTimer_ = 0.0f;
		afterimageSpawnTimer_ = 0.0f;
	}

	void BossPresentationComponent::EnsureAfterimagePool()
	{
		if (!afterimagePool_.empty()) return;
		auto* boss = dynamic_cast<BossActor*>(GetOwner());
		HumanoidVisualComponent* visual = boss ? boss->GetHumanoidVisualComponent() : nullptr;
		if (!visual) return;

		const auto& definitions = visual->GetDefinition().GetParts();
		const std::string& skinPath = visual->GetSkinTexturePath();
		afterimagePool_.resize(kAfterimagePoolSize);
		for (AfterimageSnapshot& snapshot : afterimagePool_)
		{
			snapshot.parts.reserve(definitions.size());
			for (const HumanoidPartDefinition& definition : definitions)
			{
				AfterimagePart part{};
				part.partId = definition.id;
				try
				{
					part.object = std::make_unique<Object3D>();
					part.object->Initialize(definition.modelPath);
					if (!skinPath.empty()) part.object->SetTextureForAll(skinPath);
					part.object->SetAlphaBlendEnabled(true);
					part.object->SetFrustumCullingEnabled(false);
					part.object->SetPbrEnabled(false);
					part.object->SetReflectivity(0.0f);
					part.object->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
					part.object->SetEmissiveFactor({ 0.0f, 0.0f, 0.0f, 1.0f });
				}
				catch (const std::exception&)
				{
					part.object.reset();
				}
				snapshot.parts.push_back(std::move(part));
			}
		}
	}

	void BossPresentationComponent::SpawnChargeAfterimage()
	{
		EnsureAfterimagePool();
		if (afterimagePool_.empty()) return;
		auto* boss = dynamic_cast<BossActor*>(GetOwner());
		HumanoidVisualComponent* visual = boss ? boss->GetHumanoidVisualComponent() : nullptr;
		if (!visual) return;

		AfterimageSnapshot& snapshot = afterimagePool_[nextAfterimageIndex_ % afterimagePool_.size()];
		nextAfterimageIndex_ = (nextAfterimageIndex_ + 1u) % afterimagePool_.size();
		snapshot.age = 0.0f;
		snapshot.active = false;
		for (AfterimagePart& afterimagePart : snapshot.parts)
		{
			const HumanoidVisualComponent::BodyPart* source = visual->FindPart(afterimagePart.partId);
			afterimagePart.visible = source && source->visible && source->active && source->object && afterimagePart.object;
			if (!afterimagePart.visible) continue;
			afterimagePart.worldMatrix = source->transform.worldMatrix_;
			afterimagePart.object->UpdateWithWorldMatrix(afterimagePart.worldMatrix);
			snapshot.active = true;
		}
	}

	void BossPresentationComponent::UpdateAfterimages(float deltaTime)
	{
		for (AfterimageSnapshot& snapshot : afterimagePool_)
		{
			if (!snapshot.active) continue;
			snapshot.age += deltaTime;
			if (snapshot.age >= kAfterimageLifetime)
			{
				snapshot.active = false;
				continue;
			}

			const float remain = 1.0f - Clamp01(snapshot.age / kAfterimageLifetime);
			const float alpha = 0.50f * std::pow(remain, 1.45f);
			const Vector4 color = presentedPhase_ >= 3
				? Vector4{ 1.0f, 0.18f + remain * 0.20f, 0.08f, alpha }
				: Vector4{ 0.36f + remain * 0.20f, 0.78f + remain * 0.18f, 1.0f, alpha };
			const Vector4 emissive = presentedPhase_ >= 3
				? Vector4{ 1.45f * remain, 0.04f, 0.01f, 1.0f }
				: Vector4{ 0.08f, 0.55f * remain, 1.25f * remain, 1.0f };
			for (AfterimagePart& part : snapshot.parts)
			{
				if (!part.visible || !part.object) continue;
				part.object->SetColor(color);
				part.object->SetEmissiveFactor(emissive);
				part.object->UpdateWithWorldMatrix(part.worldMatrix); // 保存済み姿勢を動かさず、MaterialとCamera定数だけ毎フレーム更新する。
			}
		}
	}

	void BossPresentationComponent::ResetAfterimages()
	{
		for (AfterimageSnapshot& snapshot : afterimagePool_)
		{
			snapshot.active = false;
			snapshot.age = 0.0f;
			for (AfterimagePart& part : snapshot.parts) part.visible = false;
		}
		nextAfterimageIndex_ = 0;
	}

	void BossPresentationComponent::EmitAttackTelegraphPulse(bool impact)
	{
		auto* boss = dynamic_cast<BossActor*>(GetOwner());
		if (!boss || activeAttackId_.empty()) return;
		const std::string_view attackId(activeAttackId_);
		Vector3 bossPosition = boss->GetPosition();
		const Vector3 targetDirection = IsChargeAttack(attackId) && chargeDirectionLocked_ ? chargeDirection_ : ResolveCurrentTargetDirection();

		if (IsChargeAttack(attackId))
		{
			if (impact)
			{
				Vector3 hitPosition = bossPosition + targetDirection * 2.4f;
				hitPosition.y -= 1.55f;
				BossAttackEffects::EmitGuardianHitEffect("BossChargeImpact", GpuParticleType::Shockwave, hitPosition, 32u, 0.95f, 0.85f, 1.45f);
				return;
			}
			static constexpr std::array<const char*, 9> emitterNames{
				"BossChargeLine01", "BossChargeLine02", "BossChargeLine03",
				"BossChargeLine04", "BossChargeLine05", "BossChargeLine06",
				"BossChargeLine07", "BossChargeLine08", "BossChargeLine09"
			};
			for (std::size_t index = 0; index < emitterNames.size(); ++index)
			{
				Vector3 linePosition = bossPosition + targetDirection * (kChargeGuideStartDistance + static_cast<float>(index) * kChargeGuideSpacing);
				linePosition.y -= 1.45f;
				BossAttackEffects::EmitGuardianTelegraphEffect(emitterNames[index], GpuParticleType::Spark, linePosition, 8u, 0.46f, 0.42f, 0.28f);
			}
			return;
		}

		if (IsAreaAttack(attackId))
		{
			Vector3 groundPosition = bossPosition;
			groundPosition.y -= 1.85f;
			const bool groundSlam = IsGroundSlamAttack(attackId);
			if (impact)
			{
				BossAttackEffects::EmitGuardianHitEffect(groundSlam ? "BossGroundSlamImpact" : "BossShockwaveImpact", GpuParticleType::Shockwave, groundPosition, groundSlam ? 72u : 54u, groundSlam ? 2.8f : 2.2f, 1.0f, groundSlam ? 2.0f : 1.6f);
			}
			else
			{
				BossAttackEffects::EmitGuardianTelegraphEffect(groundSlam ? "BossGroundSlamTelegraph" : "BossShockwaveTelegraph", GpuParticleType::Shockwave, groundPosition, groundSlam ? 14u : 10u, groundSlam ? 2.25f : 1.75f, 0.72f, 0.35f);
			}
			return;
		}

		Vector3 meleePosition = bossPosition + targetDirection * 1.65f;
		meleePosition.y += 0.35f;
		if (impact) BossAttackEffects::EmitGuardianHitEffect("BossMeleeImpact", GpuParticleType::Spark, meleePosition, 24u, 0.55f, 0.65f, 1.2f);
		else BossAttackEffects::EmitGuardianTelegraphEffect("BossMeleeTelegraph", GpuParticleType::Spark, meleePosition, 6u, 0.30f, 0.48f, 0.45f);
	}

	void BossPresentationComponent::EmitPhasePulse(bool initialBurst)
	{
		auto* boss = dynamic_cast<BossActor*>(GetOwner());
		if (!boss || presentedPhase_ < 2) return;
		Vector3 position = boss->GetPosition();
		position.y += 0.45f;
		if (initialBurst)
		{
			BossAttackEffects::EmitGuardianAttackPresenceEffect(presentedPhase_ >= 3 ? "BossPhase3Burst" : "BossPhase2Burst", presentedPhase_ >= 3 ? GpuParticleType::Spark : GpuParticleType::Ambient, position, presentedPhase_ >= 3 ? 64u : 42u, presentedPhase_ >= 3 ? 2.5f : 2.0f, 1.35f, presentedPhase_ >= 3 ? 1.75f : 0.85f);
			return;
		}
		BossAttackEffects::EmitGuardianTelegraphEffect(presentedPhase_ >= 3 ? "BossPhase3Aura" : "BossPhase2Aura", presentedPhase_ >= 3 ? GpuParticleType::Spark : GpuParticleType::Ambient, position, presentedPhase_ >= 3 ? 7u : 4u, presentedPhase_ >= 3 ? 1.65f : 1.35f, 0.85f, presentedPhase_ >= 3 ? 0.95f : 0.45f);
	}

	void BossPresentationComponent::ApplyVisualAppearance()
	{
		auto* boss = dynamic_cast<BossActor*>(GetOwner());
		HumanoidVisualComponent* visual = boss ? boss->GetHumanoidVisualComponent() : nullptr;
		if (!visual) return;

		const float pulse = 0.5f + 0.5f * std::sin(visualTime_ * (presentedPhase_ >= 3 ? 5.2f : 2.8f) * kTwoPi);
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 emissive{ 0.0f, 0.0f, 0.0f, 1.0f };
		if (presentedPhase_ == 2)
		{
			color = { 0.88f, 0.96f, 1.0f, 1.0f };
			emissive = { 0.04f, 0.16f + pulse * 0.10f, 0.34f + pulse * 0.18f, 1.0f };
		}
		else if (presentedPhase_ >= 3)
		{
			color = { 1.0f, 0.78f + pulse * 0.12f, 0.72f + pulse * 0.10f, 1.0f };
			emissive = { 0.52f + pulse * 0.42f, 0.035f, 0.02f, 1.0f };
		}

		if (attackTelegraphActive_)
		{
			const float progress = telegraphDuration_ > 0.0f ? Clamp01(telegraphElapsed_ / telegraphDuration_) : 1.0f;
			const float warningPulse = 0.5f + 0.5f * std::sin((progress * 5.0f + visualTime_ * 2.0f) * kTwoPi);
			Vector4 warningColor{ 1.0f, 0.90f, 0.35f, 1.0f };
			Vector4 warningEmissive{ 1.25f, 0.70f, 0.06f, 1.0f };
			if (IsChargeAttack(activeAttackId_))
			{
				warningColor = { 1.0f, 0.48f, 0.20f, 1.0f };
				warningEmissive = { 1.65f, 0.18f, 0.02f, 1.0f };
			}
			else if (IsAreaAttack(activeAttackId_))
			{
				warningColor = IsGroundSlamAttack(activeAttackId_) ? Vector4{ 1.0f, 0.72f, 0.20f, 1.0f } : Vector4{ 0.42f, 0.82f, 1.0f, 1.0f };
				warningEmissive = IsGroundSlamAttack(activeAttackId_) ? Vector4{ 1.45f, 0.48f, 0.02f, 1.0f } : Vector4{ 0.05f, 0.70f, 1.45f, 1.0f };
			}
			const float warningWeight = 0.28f + progress * 0.52f + warningPulse * 0.20f;
			color = LerpColor(color, warningColor, warningWeight);
			emissive = LerpColor(emissive, warningEmissive, warningWeight);
		}
		else if (chargeTrailActive_)
		{
			const Vector4 dashColor = presentedPhase_ >= 3 ? Vector4{ 1.0f, 0.25f, 0.10f, 1.0f } : Vector4{ 0.48f, 0.88f, 1.0f, 1.0f };
			const Vector4 dashEmissive = presentedPhase_ >= 3 ? Vector4{ 1.55f, 0.04f, 0.01f, 1.0f } : Vector4{ 0.05f, 0.70f, 1.45f, 1.0f };
			color = LerpColor(color, dashColor, 0.32f);
			emissive = LerpColor(emissive, dashEmissive, 0.45f);
		}

		if (phaseTransitionActive_)
		{
			const float progress = Clamp01(elapsed_ / std::max(phaseTransitionDuration_, 0.01f));
			const float transitionPulse = 0.5f + 0.5f * std::sin((progress * 4.0f + visualTime_) * kTwoPi);
			const Vector4 phaseColor = presentedPhase_ >= 3 ? Vector4{ 1.0f, 0.28f, 0.12f, 1.0f } : Vector4{ 0.35f, 0.78f, 1.0f, 1.0f };
			const Vector4 phaseEmissive = presentedPhase_ >= 3 ? Vector4{ 2.2f, 0.08f, 0.02f, 1.0f } : Vector4{ 0.08f, 1.0f, 2.0f, 1.0f };
			color = LerpColor(color, phaseColor, 0.55f + transitionPulse * 0.35f);
			emissive = LerpColor(emissive, phaseEmissive, 0.65f + transitionPulse * 0.30f);
		}
		else if (deathPresentationActive_)
		{
			const float fade = 1.0f - Clamp01(elapsed_ / std::max(deathPresentationDuration_, 0.01f));
			color = LerpColor({ 0.35f, 0.04f, 0.03f, 1.0f }, { 0.03f, 0.03f, 0.03f, 1.0f }, 1.0f - fade);
			emissive = { 1.4f * fade, 0.03f, 0.01f, 1.0f };
		}

		auto applyPart = [&color, &emissive](HumanoidVisualComponent::BodyPart& part)
		{
			if (!part.object) return;
			part.object->SetColor(color);
			part.object->SetEmissiveFactor(emissive);
		};
		applyPart(visual->GetBodyPart());
		for (HumanoidVisualComponent::BodyPart& part : visual->GetParts()) applyPart(part);
	}

	void BossPresentationComponent::StartPhaseTransition(int phase)
	{
		presentedPhase_ = std::max(1, phase);
		FinishAttackTelegraph();
		ResetAfterimages();
		phaseTransitionActive_ = true;
		elapsed_ = 0.0f;
		phaseParticleTimer_ = 0.0f;
		stateName_ = "Phase " + std::to_string(presentedPhase_);
		Actor* owner = GetOwner();
		if (auto* attack = owner ? owner->GetComponent<BossAttackComponent>() : nullptr) attack->InterruptCurrentAttack();
		if (auto* character = dynamic_cast<CharacterActor*>(owner))
		{
			if (CharacterAnimationComponent* animation = character->GetAnimationComponent()) animation->Play("Boss.PhaseTransition", phaseTransitionDuration_, false);
		}
		if (auto* boss = dynamic_cast<BossActor*>(owner))
		{
			if (TextComponent* label = boss->GetHealthLabelComponent()) label->SetText("BOSS PHASE " + std::to_string(presentedPhase_));
		}
		AudioManager::GetInstance()->PlaySE("enemy_death.mp3", presentedPhase_ >= 3 ? 0.32f : 0.25f, presentedPhase_ >= 3 ? 0.58f : 0.74f);
		EmitPhasePulse(true);
		ApplyVisualAppearance();
	}

	void BossPresentationComponent::StartCameraShake(float duration, float amplitude, float frequency)
	{
		if (duration <= 0.0f || amplitude <= 0.0f) return;
		if (cameraShakeTimer_ > 0.0f && amplitude < cameraShakeAmplitude_) return;
		cameraShakeDuration_ = cameraShakeTimer_ = duration;
		cameraShakeAmplitude_ = amplitude;
		cameraShakeFrequency_ = std::max(1.0f, frequency);
		cameraShakeSeed_ += 1.73f;
	}

	void BossPresentationComponent::UpdateCameraShake(float deltaTime)
	{
		if (cameraShakeTimer_ <= 0.0f || cameraShakeDuration_ <= 0.0f) return;
		cameraShakeTimer_ = std::max(0.0f, cameraShakeTimer_ - deltaTime);
		::IPlayerRuntime* player = ::IPlayerRuntime::GetActiveRuntime();
		Camera* camera = player ? player->GetCamera() : nullptr;
		if (!camera) return;
		const float rate = Clamp01(cameraShakeTimer_ / cameraShakeDuration_);
		const float time = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
		const float amplitude = cameraShakeAmplitude_ * rate * rate;
		const Vector3 offset{
			std::sin(time * 1.47f) * amplitude,
			std::cos(time * 1.11f) * amplitude * 0.45f,
			std::sin(time * 0.79f) * amplitude * 0.25f
		};
		camera->SetTranslate(camera->GetTranslate() + offset);
		camera->Update(); // 攻撃着地の短いShakeだけをここで加え、Phase移行はController側の既存処理を使う。
	}
} // namespace Ken4lowEngine
