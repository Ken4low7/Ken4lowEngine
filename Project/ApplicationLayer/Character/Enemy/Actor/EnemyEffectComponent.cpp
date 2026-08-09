#define NOMINMAX
#include "EnemyEffectComponent.h"

#include <Actor.h>
#include <AudioManager.h>
#include <Object3D.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
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
	}

	void EnemyEffectComponent::Initialize()
	{
		effectSystem_.Initialize();
		ResetEffectState();
	}

	void EnemyEffectComponent::Update(float deltaTime)
	{
		if (spawnPresentationPending_) StartPendingSpawnEffect();
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

		if (spawnEffectActive_) UpdateSpawnPresentation(deltaTime);
		if (!deathEffectActive_) return;

		deathTimer_ += deltaTime;
		if (deathTimer_ < deathPresentationDuration_) return;

		deathEffectActive_ = false;
		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>()) visual->SetAllPartsVisible(false);
		}
	}

	void EnemyEffectComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵Effect");
		ImGui::Text("Spawn: %d / Hit: %d / Death: %d", spawnEffectCount_, hitEffectCount_, deathEffectCount_);
		ImGui::Text("生成演出: %s %.2f / %.2f", spawnEffectActive_ ? "再生中" : "停止", spawnTimer_, spawnPresentationDuration_);
		ImGui::Text("死亡演出: %s %.2f / %.2f", deathEffectActive_ ? "再生中" : "停止", deathTimer_, deathPresentationDuration_);
#endif
	}

	void EnemyEffectComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["SpawnPresentationDuration"] = spawnPresentationDuration_;
		outJson["DeathPresentationDuration"] = deathPresentationDuration_;
	}

	void EnemyEffectComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		spawnPresentationDuration_ = std::clamp(inJson.value("SpawnPresentationDuration", spawnPresentationDuration_), 0.15f, 2.0f);
		deathPresentationDuration_ = std::max(0.1f, inJson.value("DeathPresentationDuration", deathPresentationDuration_));
	}

	void EnemyEffectComponent::TriggerSpawnEffect(const Vector3& worldPosition)
	{
		if (deathEffectActive_) return;
		spawnPresentationPending_ = false;
		spawnTimer_ = 0.0f;
		spawnEffectActive_ = true;
		effectSystem_.SpawnAppearEffect(worldPosition);
		AudioManager::GetInstance()->PlaySE("enemy_hit.mp3", 0.09f, 1.30f);
		++spawnEffectCount_;

		Actor* owner = GetOwner();
		if (!owner) return;
		if (CharacterHealthComponent* health = owner->GetComponent<CharacterHealthComponent>())
		{
			spawnPreviousInvulnerable_ = health->IsInvulnerable();
			health->SetInvulnerable(true);
		}
		if (HumanoidVisualComponent* visual = owner->GetComponent<HumanoidVisualComponent>())
		{
			spawnBaseScale_ = visual->GetLocalScale();
			if (Vector3::Length(spawnBaseScale_) <= 0.0001f) spawnBaseScale_ = { 1.0f, 1.0f, 1.0f };
			visual->SetLocalScale(spawnBaseScale_ * 0.22f);
			ApplyVisualColor(*visual, { 0.30f, 0.80f, 1.0f, 1.0f }, { 0.10f, 1.0f, 1.8f, 1.0f });
		}
	}

	void EnemyEffectComponent::TriggerHitEffect(const Vector3& worldPosition)
	{
		effectSystem_.SpawnHitEffect(worldPosition);
		++hitEffectCount_;
	}

	void EnemyEffectComponent::TriggerDeathEffect(const Vector3& worldPosition)
	{
		if (deathEffectActive_) return;
		if (spawnEffectActive_) FinishSpawnPresentation();
		effectSystem_.SpawnDeathEffect(worldPosition);
		deathEffectActive_ = true;
		deathTimer_ = 0.0f;
		++deathEffectCount_;

		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>())
			{
				ApplyVisualColor(*visual, { 1.0f, 0.35f, 0.25f, 1.0f }, { 0.35f, 0.02f, 0.01f, 1.0f });
			}
		}
	}

	void EnemyEffectComponent::ResetEffectState()
	{
		if (spawnEffectActive_) FinishSpawnPresentation();
		spawnTimer_ = 0.0f;
		deathTimer_ = 0.0f;
		spawnEffectCount_ = 0;
		hitEffectCount_ = 0;
		deathEffectCount_ = 0;
		spawnPresentationPending_ = true;
		spawnEffectActive_ = false;
		spawnPreviousInvulnerable_ = false;
		deathEffectActive_ = false;
		if (Actor* owner = GetOwner())
		{
			if (auto* visual = owner->GetComponent<HumanoidVisualComponent>())
			{
				visual->SetAllPartsVisible(true);
				ApplyVisualColor(*visual, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
			}
		}
	}

	void EnemyEffectComponent::StartPendingSpawnEffect()
	{
		Actor* owner = GetOwner();
		if (!owner) return;
		const SceneComponent* root = owner->GetRootComponent();
		TriggerSpawnEffect(root ? root->GetWorldPosition() : Vector3{}); // CharacterWorldがSpawn位置を反映した最初の更新で演出を始める。
	}

	void EnemyEffectComponent::UpdateSpawnPresentation(float deltaTime)
	{
		spawnTimer_ += deltaTime;
		const float progress = Clamp01(spawnTimer_ / std::max(spawnPresentationDuration_, 0.01f));
		const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
		const float scaleRate = 0.22f + eased * 0.78f + std::sin(progress * 3.14159265f) * 0.10f;

		Actor* owner = GetOwner();
		HumanoidVisualComponent* visual = owner ? owner->GetComponent<HumanoidVisualComponent>() : nullptr;
		if (visual)
		{
			visual->SetLocalScale(spawnBaseScale_ * scaleRate);
			const Vector4 color = LerpColor({ 0.30f, 0.80f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, eased);
			const Vector4 emissive = LerpColor({ 0.10f, 1.0f, 1.8f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, progress);
			ApplyVisualColor(*visual, color, emissive);
		}

		if (progress >= 1.0f) FinishSpawnPresentation();
	}

	void EnemyEffectComponent::FinishSpawnPresentation()
	{
		if (!spawnEffectActive_) return;
		spawnEffectActive_ = false;
		spawnTimer_ = spawnPresentationDuration_;

		Actor* owner = GetOwner();
		if (!owner) return;
		if (CharacterHealthComponent* health = owner->GetComponent<CharacterHealthComponent>())
		{
			health->SetInvulnerable(spawnPreviousInvulnerable_);
		}
		if (HumanoidVisualComponent* visual = owner->GetComponent<HumanoidVisualComponent>())
		{
			visual->SetLocalScale(spawnBaseScale_);
			ApplyVisualColor(*visual, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
		}
		spawnPreviousInvulnerable_ = false;
	}

	void EnemyEffectComponent::ApplyVisualColor(HumanoidVisualComponent& visual, const Vector4& color, const Vector4& emissive)
	{
		auto applyPart = [&color, &emissive](HumanoidVisualComponent::BodyPart& part)
		{
			if (!part.object) return;
			part.object->SetColor(color);
			part.object->SetEmissiveFactor(emissive);
		};
		applyPart(visual.GetBodyPart());
		for (HumanoidVisualComponent::BodyPart& part : visual.GetParts()) applyPart(part); // 胴体を含む全部位を同じ発光で展開する。
	}
} // namespace Ken4lowEngine
