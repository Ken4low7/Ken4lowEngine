#pragma once

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"

#include <Actor.h>
#include <ActorWorld.h>
#include <Collider.h>
#include <ColliderComponent.h>
#include <GaugeComponent.h>
#include <InstancedModelComponent.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <SceneComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// 高架上の足場へ乗ると警告後に崩落する、Stage 4中盤専用のInstancing Actor。
class Stage4CollapsingAthleticActor final : public K4E::Actor
{
private:
	enum class PlatformState
	{
		Idle,
		Warning,
		Falling,
		Fallen,
	};

	struct PlatformSpec
	{
		K4E::Vector3 position;
		K4E::Vector3 scale;
		float yaw = 0.0f;
		float warningSeconds = 2.5f;
	};

	struct PlatformRuntime
	{
		PlatformState state = PlatformState::Idle;
		float stateTimer = 0.0f;
	};

	static constexpr size_t kPlatformCount = 6u;
	static constexpr size_t kVisualInstanceCount = kPlatformCount * 3u;

public:
	std::string GetClassTypeName() const override { return "Stage4CollapsingAthleticActor"; }

	void Initialize() override
	{
		auto& root = CreateRootComponent<K4E::SceneComponent>();
		root.SetName("Stage 4 Athletic Root");
		root.SetUpdateOrder(-100);

		auto& visual = AddComponent<K4E::InstancedModelComponent>();
		visual.SetName("Stage 4 Athletic Instances");
		visual.SetUpdateOrder(-20);
		visual.SetDrawOrder(2);
		visual.SetModelPath("Sample/cube.gltf");
		visual.SetInstanceCount(static_cast<int>(kVisualInstanceCount));
		visual.SetCastShadowEnabled(true);
		visual.AttachTo(&root);
		visualComponent_ = &visual;

		const auto& specs = GetPlatformSpecs();
		for (size_t index = 0; index < specs.size(); ++index)
		{
			auto& platformRoot = AddComponent<K4E::SceneComponent>();
			platformRoot.SetName("Stage 4 Athletic Platform Root " + std::to_string(index + 1));
			platformRoot.SetUpdateOrder(-90 + static_cast<int>(index));
			platformRoot.SetLocalPosition(specs[index].position);
			platformRoot.SetLocalRotation({ 0.0f, specs[index].yaw, 0.0f });
			platformRoot.AttachTo(&root);
			platformRoots_[index] = &platformRoot;

			auto& collider = AddComponent<K4E::ColliderComponent>();
			collider.SetName("Stage 4 Athletic Floor Collider " + std::to_string(index + 1));
			collider.SetUpdateOrder(-60 + static_cast<int>(index));
			collider.SetShapeType(K4E::ECollisionShapeType::OBB);
			collider.SetHalfSize(specs[index].scale);
			collider.SetCollisionLayer(K4E::PhysicsCollisionLayer::WorldStatic);
			collider.SetCollisionTag("Floor");
			collider.SetIsTrigger(false);
			collider.AttachTo(&platformRoot);
			platformColliders_[index] = &collider;
		}

		auto& rigidbody = AddComponent<K4E::RigidbodyComponent>();
		rigidbody.SetName("Stage 4 Athletic Rigidbody");
		rigidbody.SetUpdateOrder(-120);
		rigidbody.SetBodyType(K4E::BodyType::Static);
		rigidbody.SetUseGravity(false);
		rigidbody.SetSleepEnabled(false);

		SetName("Stage4CollapsingAthletic");
		SetLayer("StageObjective");
		AddTag("Stage4Athletic");
		K4E::Actor::Initialize();

		ResetAll();
		ApplyVisualTransforms();
		if (visualComponent_) visualComponent_->Update(0.0f);
	}

	void Update(float deltaTime) override
	{
		ApplyVisualTransforms();
		K4E::Actor::Update(deltaTime);
	}

	void Advance(const K4E::Vector3& playerPosition, float deltaTime)
	{
		const float safeDeltaTime = std::max(0.0f, deltaTime);
		const auto& specs = GetPlatformSpecs();

		for (size_t index = 0; index < platforms_.size(); ++index)
		{
			PlatformRuntime& runtime = platforms_[index];
			const PlatformSpec& spec = specs[index];

			if (runtime.state == PlatformState::Idle && IsPlayerOnPlatform(playerPosition, spec))
			{
				runtime.state = PlatformState::Warning;
				runtime.stateTimer = 0.0f; // 着地した足場だけを起動し、先の足場まで一斉に崩れる理不尽さを避ける。
			}

			if (runtime.state == PlatformState::Warning)
			{
				runtime.stateTimer += safeDeltaTime;
				if (runtime.stateTimer >= spec.warningSeconds)
				{
					runtime.state = PlatformState::Falling;
					runtime.stateTimer = 0.0f;
					SetPlatformColliderEnabled(index, false);
				}
			}
			else if (runtime.state == PlatformState::Falling)
			{
				runtime.stateTimer += safeDeltaTime;
				if (runtime.stateTimer >= fallDurationSeconds_)
				{
					runtime.state = PlatformState::Fallen;
					runtime.stateTimer = fallDurationSeconds_;
				}
			}
		}
	}

	void ResetAll()
	{
		for (size_t index = 0; index < platforms_.size(); ++index)
		{
			platforms_[index] = {};
			SetPlatformColliderEnabled(index, true);
		}
		ApplyVisualTransforms();
	}

	bool HasCollapsedPlatform() const
	{
		return std::any_of(platforms_.begin(), platforms_.end(), [](const PlatformRuntime& runtime)
			{
				return runtime.state == PlatformState::Falling || runtime.state == PlatformState::Fallen;
			});
	}

	bool HasActiveWarning() const
	{
		return std::any_of(platforms_.begin(), platforms_.end(), [](const PlatformRuntime& runtime)
			{
				return runtime.state == PlatformState::Warning;
			});
	}

	float GetShortestWarningRemaining() const
	{
		float remaining = 1.0e30f;
		const auto& specs = GetPlatformSpecs();
		for (size_t index = 0; index < platforms_.size(); ++index)
		{
			if (platforms_[index].state != PlatformState::Warning) continue;
			remaining = std::min(remaining, std::max(0.0f, specs[index].warningSeconds - platforms_[index].stateTimer));
		}
		return remaining == 1.0e30f ? 0.0f : remaining;
	}

	float GetShortestWarningRate() const
	{
		float rate = 1.0f;
		bool found = false;
		const auto& specs = GetPlatformSpecs();
		for (size_t index = 0; index < platforms_.size(); ++index)
		{
			if (platforms_[index].state != PlatformState::Warning) continue;
			const float current = 1.0f - platforms_[index].stateTimer / std::max(0.05f, specs[index].warningSeconds);
			rate = std::min(rate, std::clamp(current, 0.0f, 1.0f));
			found = true;
		}
		return found ? rate : 1.0f;
	}

private:
	static const std::array<PlatformSpec, kPlatformCount>& GetPlatformSpecs()
	{
		static const std::array<PlatformSpec, kPlatformCount> specs = {{
			{ { 0.0f, 10.0f, 107.0f }, { 5.5f, 0.55f, 3.4f }, 0.00f, 3.0f },
			{ { -3.2f, 10.4f, 115.0f }, { 4.8f, 0.50f, 3.2f }, 0.05f, 2.8f },
			{ { 3.5f, 10.8f, 123.0f }, { 4.5f, 0.50f, 3.2f }, -0.06f, 2.6f },
			{ { -2.8f, 10.4f, 131.0f }, { 4.4f, 0.50f, 3.2f }, 0.07f, 2.5f },
			{ { 4.0f, 10.8f, 139.0f }, { 4.5f, 0.50f, 3.2f }, -0.07f, 2.4f },
			{ { 0.0f, 10.0f, 147.0f }, { 5.5f, 0.55f, 3.4f }, 0.00f, 2.3f }
		}};
		return specs;
	}

	static float SmoothStep(float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return clamped * clamped * (3.0f - 2.0f * clamped);
	}

	static bool IsPlayerOnPlatform(const K4E::Vector3& playerPosition, const PlatformSpec& spec)
	{
		const float platformTop = spec.position.y + spec.scale.y;
		return std::fabs(playerPosition.x - spec.position.x) <= spec.scale.x + 0.75f &&
			std::fabs(playerPosition.z - spec.position.z) <= spec.scale.z + 0.75f &&
			playerPosition.y >= platformTop - 0.50f &&
			playerPosition.y <= platformTop + 4.50f;
	}

	void ApplyVisualTransforms()
	{
		if (!visualComponent_) return;
		const auto& specs = GetPlatformSpecs();

		for (size_t index = 0; index < platforms_.size(); ++index)
		{
			const PlatformSpec& spec = specs[index];
			const PlatformRuntime& runtime = platforms_[index];
			K4E::Vector3 position = spec.position;
			K4E::Vector3 rotation{ 0.0f, spec.yaw, 0.0f };
			K4E::Vector4 slabColor{ 0.15f, 0.16f, 0.18f, 1.0f };

			if (runtime.state == PlatformState::Warning)
			{
				const float warningRate = runtime.stateTimer / std::max(0.05f, spec.warningSeconds);
				const float pulse = 0.5f + 0.5f * std::sin(runtime.stateTimer * 18.0f);
				position.y += std::sin(runtime.stateTimer * 31.0f + static_cast<float>(index)) * 0.045f * warningRate;
				rotation.z += std::sin(runtime.stateTimer * 23.0f) * 0.025f * warningRate;
				slabColor = { 0.32f + pulse * 0.18f, 0.16f, 0.08f, 1.0f };
			}
			else if (runtime.state == PlatformState::Falling || runtime.state == PlatformState::Fallen)
			{
				const float fallRate = SmoothStep(runtime.stateTimer / std::max(0.05f, fallDurationSeconds_));
				const float side = index % 2u == 0u ? 1.0f : -1.0f;
				position.y -= 13.0f * fallRate;
				position.x += side * 1.6f * fallRate;
				rotation.x += side * 1.05f * fallRate;
				rotation.z += side * 0.55f * fallRate;
				slabColor = { 0.20f, 0.10f, 0.06f, 1.0f };
			}

			if (platformRoots_[index])
			{
				platformRoots_[index]->SetLocalPosition(position);
				platformRoots_[index]->SetLocalRotation(rotation);
			}

			K4E::InstancedModelComponent::InstanceTransform slab{};
			slab.position = position;
			slab.rotation = rotation;
			slab.scale = spec.scale;
			slab.color = slabColor;
			visualComponent_->SetInstanceLocalTransform(index, slab);

			K4E::InstancedModelComponent::InstanceTransform stripe{};
			stripe.position = position + K4E::Vector3{ 0.0f, spec.scale.y + 0.055f, 0.0f };
			stripe.rotation = rotation;
			stripe.scale = { std::min(1.0f, spec.scale.x * 0.20f), 0.03f, std::min(1.6f, spec.scale.z * 0.50f) };
			stripe.color = runtime.state == PlatformState::Warning
				? K4E::Vector4{ 1.0f, 0.24f, 0.08f, 1.0f }
				: K4E::Vector4{ 0.95f, 0.62f, 0.08f, 1.0f };
			visualComponent_->SetInstanceLocalTransform(kPlatformCount + index, stripe);

			const float railSide = index % 2u == 0u ? -1.0f : 1.0f;
			K4E::InstancedModelComponent::InstanceTransform rail{};
			rail.position = position + K4E::Vector3{ railSide * (spec.scale.x - 0.22f), spec.scale.y + 0.48f, 0.0f };
			rail.rotation = rotation + K4E::Vector3{ 0.06f * railSide, 0.0f, 0.10f * railSide };
			rail.scale = { 0.16f, 0.20f, std::max(1.6f, spec.scale.z * 0.72f) };
			rail.color = { 0.25f, 0.26f, 0.29f, 1.0f };
			visualComponent_->SetInstanceLocalTransform(kPlatformCount * 2u + index, rail);
		}
	}

	void SetPlatformColliderEnabled(size_t index, bool enabled)
	{
		if (index >= platformColliders_.size()) return;
		K4E::ColliderComponent* colliderComponent = platformColliders_[index];
		if (!colliderComponent) return;
		colliderComponent->SetActive(enabled);
		if (K4E::Collider* collider = colliderComponent->GetCollider()) collider->SetEnabled(enabled);
	}

	K4E::InstancedModelComponent* visualComponent_ = nullptr;
	std::array<K4E::SceneComponent*, kPlatformCount> platformRoots_{};
	std::array<K4E::ColliderComponent*, kPlatformCount> platformColliders_{};
	std::array<PlatformRuntime, kPlatformCount> platforms_{};
	float fallDurationSeconds_ = 0.90f;
};

/// ボスバーと同じ上中央位置へ崩落カウントダウンを表示するActor。
class Stage4AthleticGaugeActor final : public K4E::Actor
{
public:
	std::string GetClassTypeName() const override { return "Stage4AthleticGaugeActor"; }

	void Initialize() override
	{
		auto& root = CreateRootComponent<K4E::SceneComponent>();
		root.SetName("Stage 4 Athletic Gauge Root");

		auto& gauge = AddComponent<K4E::GaugeComponent>();
		gauge.SetName("Stage 4 Athletic Boss Style Gauge");
		gauge.SetDrawOrder(1420);
		gauge.SetPosition({ 580.0f, 58.0f });
		gauge.SetSize({ 760.0f, 22.0f });
		gauge.SetMaxValue(1.0f);
		gauge.SetValue(1.0f);
		gauge.SetBackgroundColor({ 0.025f, 0.018f, 0.025f, 0.94f });
		gauge.SetFillColor({ 0.96f, 0.28f, 0.10f, 0.98f });
		gauge.SetBorderColor({ 0.92f, 0.94f, 1.0f, 0.96f });
		gauge.SetBorderThickness(3.0f);
		gauge.SetVisible(false);
		gauge_ = &gauge;

		auto& title = AddComponent<K4E::TextComponent>();
		title.SetName("Stage 4 Athletic Gauge Title");
		title.SetDrawOrder(1421);
		title.SetPosition({ 960.0f, 33.0f });
		title.SetAnchor({ 0.5f, 0.5f });
		title.SetFontName("DotGothic16");
		title.SetFontSize(27.0f);
		title.SetColor({ 0.98f, 0.96f, 0.92f, 1.0f });
		title.SetVisible(false);
		title_ = &title;

		SetName("Stage4AthleticGauge");
		SetLayer("StageUI");
		AddTag("Stage4AthleticGauge");
		K4E::Actor::Initialize();
	}

	void SetDisplay(bool visible, const std::string& text, float progress, const K4E::Vector4& color)
	{
		if (gauge_)
		{
			gauge_->SetVisible(visible);
			gauge_->SetValue(std::clamp(progress, 0.0f, 1.0f));
			gauge_->SetFillColor(color);
		}
		if (title_)
		{
			title_->SetVisible(visible);
			title_->SetText(text);
		}
	}

private:
	K4E::GaugeComponent* gauge_ = nullptr;
	K4E::TextComponent* title_ = nullptr;
};

/// Stage 4中盤への進入、足場崩落、落下後の再挑戦、突破表示を管理する。
class Stage4AthleticRuntime final
{
public:
	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		active_ = stageContext.GetCurrentStageAssets().jsonPath.find("fps_stage03.json") != std::string::npos;
	}

	void Finalize()
	{
		if (athleticActor_) athleticActor_->Destroy();
		if (gaugeActor_) gaugeActor_->Destroy();
		athleticActor_ = nullptr;
		gaugeActor_ = nullptr;
		completionTimer_ = 0.0f;
		sectionEntered_ = false;
		complete_ = false;
		active_ = false;
	}

	void Update(IPlayerRuntime* player, K4E::ActorWorld& actorWorld, float deltaTime)
	{
		if (!active_) return;
		SpawnActorsIfNeeded(actorWorld);
		if (!athleticActor_ || !gaugeActor_) return;

		const float safeDeltaTime = std::max(0.0f, deltaTime);
		if (!player)
		{
			gaugeActor_->SetDisplay(false, {}, 0.0f, { 0.96f, 0.28f, 0.10f, 0.98f });
			return;
		}

		const K4E::Vector3 position = player->GetWorldPosition();
		if (!complete_ && athleticActor_->HasCollapsedPlatform() && position.z < resetSouthZ_ && position.y < lowerRouteHeightThreshold_)
		{
			athleticActor_->ResetAll();
			sectionEntered_ = false;
		}

		const bool insideSection = position.z >= sectionStartZ_ && position.z <= sectionEndZ_;
		if (insideSection && !complete_)
		{
			sectionEntered_ = true;
			athleticActor_->Advance(position, safeDeltaTime);
		}

		if (!complete_ && position.z >= completionZ_ && position.y >= upperRouteHeightThreshold_)
		{
			complete_ = true;
			completionTimer_ = 2.6f;
		}

		UpdateGauge(position, insideSection, safeDeltaTime);
	}

	bool IsActive() const { return active_; }
	bool IsComplete() const { return complete_; }

private:
	void SpawnActorsIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (!athleticActor_)
		{
			auto& actor = actorWorld.SpawnActor<Stage4CollapsingAthleticActor>();
			actor.Update(0.0f);
			athleticActor_ = &actor;
		}
		if (!gaugeActor_)
		{
			auto& actor = actorWorld.SpawnActor<Stage4AthleticGaugeActor>();
			actor.SetDisplay(false, {}, 1.0f, { 0.96f, 0.28f, 0.10f, 0.98f });
			gaugeActor_ = &actor;
		}
	}

	void UpdateGauge(const K4E::Vector3& playerPosition, bool insideSection, float deltaTime)
	{
		if (complete_)
		{
			completionTimer_ = std::max(0.0f, completionTimer_ - deltaTime);
			gaugeActor_->SetDisplay(
				completionTimer_ > 0.0f,
				"崩落高架区画を突破",
				1.0f,
				{ 0.22f, 0.94f, 0.42f, 0.98f });
			return;
		}

		if (sectionEntered_ && playerPosition.y < lowerRouteHeightThreshold_ && playerPosition.z >= resetSouthZ_)
		{
			gaugeActor_->SetDisplay(
				true,
				"落下　南側の進入口へ戻れ",
				0.0f,
				{ 0.94f, 0.34f, 0.12f, 0.98f });
			return;
		}

		if (!insideSection)
		{
			gaugeActor_->SetDisplay(false, {}, 1.0f, { 0.96f, 0.28f, 0.10f, 0.98f });
			return;
		}

		if (athleticActor_->HasActiveWarning())
		{
			char label[96]{};
			std::snprintf(label, sizeof(label), "足場崩落まで %.1f秒", athleticActor_->GetShortestWarningRemaining());
			gaugeActor_->SetDisplay(
				true,
				label,
				athleticActor_->GetShortestWarningRate(),
				{ 0.96f, 0.28f, 0.10f, 0.98f });
			return;
		}

		gaugeActor_->SetDisplay(
			true,
			"崩落高架を走り抜けろ",
			1.0f,
			{ 0.96f, 0.62f, 0.10f, 0.98f });
	}

	Stage4CollapsingAthleticActor* athleticActor_ = nullptr;
	Stage4AthleticGaugeActor* gaugeActor_ = nullptr;
	float completionTimer_ = 0.0f;
	float sectionStartZ_ = 99.0f;
	float sectionEndZ_ = 166.0f;
	float resetSouthZ_ = 100.0f;
	float completionZ_ = 150.5f;
	float lowerRouteHeightThreshold_ = 9.5f;
	float upperRouteHeightThreshold_ = 9.5f;
	bool sectionEntered_ = false;
	bool complete_ = false;
	bool active_ = false;
};
