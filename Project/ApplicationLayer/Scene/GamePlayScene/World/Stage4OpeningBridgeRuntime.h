#pragma once

#include "ApplicationLayer/Character/CharacterWorld/CharacterWorld.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"
#include "Stage2DeviceActor.h"

#include <Actor.h>
#include <Camera.h>
#include <Collider.h>
#include <ColliderComponent.h>
#include <GaugeComponent.h>
#include <Input.h>
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
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// 崩落都市の中央陥没へ展開される6分割橋を、動的Instanceと床Colliderで管理するActor。
class Stage4CollapsedBridgeActor final : public K4E::Actor
{
private:
	struct SlabSpec
	{
		K4E::Vector3 position;
		K4E::Vector3 scale;
		float yaw = 0.0f;
	};

	static constexpr size_t kSlabCount = 6u;
	static constexpr size_t kVisualInstanceCount = kSlabCount * 3u;

public:
	std::string GetClassTypeName() const override { return "Stage4CollapsedBridgeActor"; }

	void Initialize() override
	{
		auto& root = CreateRootComponent<K4E::SceneComponent>();
		root.SetName("Stage 4 Collapsed Bridge Root");
		root.SetUpdateOrder(-100);

		auto& visual = AddComponent<K4E::InstancedModelComponent>();
		visual.SetName("Stage 4 Collapsed Bridge Instances");
		visual.SetUpdateOrder(-20);
		visual.SetDrawOrder(2);
		visual.SetModelPath("Sample/cube.gltf");
		visual.SetInstanceCount(static_cast<int>(kVisualInstanceCount));
		visual.SetCastShadowEnabled(true);
		visual.AttachTo(&root);
		visualComponent_ = &visual;

		const auto& specs = GetSlabSpecs();
		for (size_t index = 0; index < specs.size(); ++index)
		{
			auto& slabRoot = AddComponent<K4E::SceneComponent>();
			slabRoot.SetName("Stage 4 Bridge Slab Root " + std::to_string(index + 1));
			slabRoot.SetUpdateOrder(-90 + static_cast<int>(index));
			slabRoot.SetLocalPosition(specs[index].position);
			slabRoot.SetLocalRotation({ 0.0f, specs[index].yaw, 0.0f });
			slabRoot.AttachTo(&root);
			slabRoots_[index] = &slabRoot;

			auto& collider = AddComponent<K4E::ColliderComponent>();
			collider.SetName("Stage 4 Bridge Floor Collider " + std::to_string(index + 1));
			collider.SetUpdateOrder(-60 + static_cast<int>(index));
			collider.SetShapeType(K4E::ECollisionShapeType::OBB);
			collider.SetHalfSize(specs[index].scale);
			collider.SetCollisionLayer(K4E::PhysicsCollisionLayer::WorldStatic);
			collider.SetCollisionTag("Floor");
			collider.SetIsTrigger(false);
			collider.AttachTo(&slabRoot);
			slabColliders_[index] = &collider;
		}

		auto& rigidbody = AddComponent<K4E::RigidbodyComponent>();
		rigidbody.SetName("Stage 4 Collapsed Bridge Rigidbody");
		rigidbody.SetUpdateOrder(-120);
		rigidbody.SetBodyType(K4E::BodyType::Static);
		rigidbody.SetUseGravity(false);
		rigidbody.SetSleepEnabled(false);

		SetName("Stage4CollapsedBridge");
		SetLayer("StageObjective");
		AddTag("Stage4Bridge");
		K4E::Actor::Initialize();

		SetBridgeCollidersEnabled(false);
		ApplyVisualTransforms();
		if (visualComponent_) visualComponent_->Update(0.0f);
	}

	void Update(float deltaTime) override
	{
		if (opening_ && openProgress_ < 1.0f)
		{
			openProgress_ = std::min(1.0f, openProgress_ + std::max(0.0f, deltaTime) / std::max(0.05f, openDuration_));
			if (openProgress_ >= 1.0f)
			{
				SetBridgeCollidersEnabled(true); // 橋が固定された瞬間だけ床Colliderを有効にし、展開中の見えない足場を防ぐ。
			}
		}

		ApplyVisualTransforms();
		K4E::Actor::Update(deltaTime);
	}

	void RequestOpen()
	{
		if (opening_ || openProgress_ >= 1.0f) return;
		opening_ = true;
	}

	bool IsOpening() const { return opening_ && openProgress_ < 1.0f; }
	bool IsOpen() const { return openProgress_ >= 1.0f; }
	float GetOpenProgress() const { return openProgress_; }

private:
	static const std::array<SlabSpec, kSlabCount>& GetSlabSpecs()
	{
		static const std::array<SlabSpec, kSlabCount> specs = {{
			{ { 0.0f, 0.20f, -52.0f }, { 3.4f, 0.20f, 3.0f }, 0.00f },
			{ { -0.4f, 0.35f, -44.0f }, { 3.2f, 0.30f, 5.0f }, 0.03f },
			{ { 0.5f, 0.50f, -34.0f }, { 3.2f, 0.35f, 5.0f }, -0.04f },
			{ { -0.5f, 0.50f, -24.0f }, { 3.2f, 0.35f, 5.0f }, 0.05f },
			{ { 0.4f, 0.35f, -14.0f }, { 3.2f, 0.30f, 5.0f }, -0.03f },
			{ { 0.0f, 0.20f, -6.0f }, { 3.4f, 0.20f, 3.0f }, 0.00f }
		}};
		return specs;
	}

	static float SmoothStep(float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return clamped * clamped * (3.0f - 2.0f * clamped);
	}

	void ApplyVisualTransforms()
	{
		if (!visualComponent_) return;
		const auto& specs = GetSlabSpecs();

		for (size_t index = 0; index < specs.size(); ++index)
		{
			const float localProgress = SmoothStep(openProgress_ * 1.28f - static_cast<float>(index) * 0.055f);
			const K4E::Vector3 closedPosition = specs[index].position + K4E::Vector3{
				0.0f,
				-8.0f - static_cast<float>(index % 2u) * 0.6f,
				0.0f
			};
			const K4E::Vector3 currentPosition = closedPosition + (specs[index].position - closedPosition) * localProgress;
			const float tiltSign = (index % 2u == 0u) ? 1.0f : -1.0f;

			K4E::InstancedModelComponent::InstanceTransform slab{};
			slab.position = currentPosition;
			slab.rotation = {
				(1.0f - localProgress) * 0.22f * tiltSign,
				specs[index].yaw,
				(1.0f - localProgress) * 0.09f * tiltSign
			};
			slab.scale = specs[index].scale;
			slab.color = { 0.13f, 0.14f, 0.16f, 1.0f };
			visualComponent_->SetInstanceLocalTransform(index, slab);

			K4E::InstancedModelComponent::InstanceTransform guide{};
			guide.position = currentPosition + K4E::Vector3{ 0.0f, specs[index].scale.y + 0.055f, 0.0f };
			guide.rotation = slab.rotation;
			guide.scale = { 0.46f, 0.03f, std::min(1.45f, specs[index].scale.z * 0.42f) };
			guide.color = { 0.18f, 0.72f, 0.95f, 1.0f };
			visualComponent_->SetInstanceLocalTransform(kSlabCount + index, guide);

			const float railSide = index % 2u == 0u ? -1.0f : 1.0f;
			K4E::InstancedModelComponent::InstanceTransform rail{};
			rail.position = currentPosition + K4E::Vector3{
				railSide * std::max(0.5f, specs[index].scale.x - 0.20f),
				specs[index].scale.y + 0.45f,
				0.0f
			};
			rail.rotation = {
				slab.rotation.x + 0.08f * railSide,
				specs[index].yaw,
				slab.rotation.z + 0.14f * railSide
			};
			rail.scale = { 0.16f, 0.18f, std::min(3.8f, specs[index].scale.z * 0.72f) };
			rail.color = { 0.24f, 0.25f, 0.28f, 1.0f };
			visualComponent_->SetInstanceLocalTransform(kSlabCount * 2u + index, rail);
		}
	}

	void SetBridgeCollidersEnabled(bool enabled)
	{
		for (K4E::ColliderComponent* colliderComponent : slabColliders_)
		{
			if (!colliderComponent) continue;
			colliderComponent->SetActive(enabled);
			if (K4E::Collider* collider = colliderComponent->GetCollider()) collider->SetEnabled(enabled);
		}
	}

	K4E::InstancedModelComponent* visualComponent_ = nullptr;
	std::array<K4E::SceneComponent*, kSlabCount> slabRoots_{};
	std::array<K4E::ColliderComponent*, kSlabCount> slabColliders_{};
	float openDuration_ = 3.4f;
	float openProgress_ = 0.0f;
	bool opening_ = false;
};

/// ボスバーと同じ上中央配置で、序盤ギミックの進捗と指示を表示するActor。
class Stage4OpeningGaugeActor final : public K4E::Actor
{
public:
	std::string GetClassTypeName() const override { return "Stage4OpeningGaugeActor"; }

	void Initialize() override
	{
		auto& root = CreateRootComponent<K4E::SceneComponent>();
		root.SetName("Stage 4 Opening Gauge Root");

		auto& gauge = AddComponent<K4E::GaugeComponent>();
		gauge.SetName("Stage 4 Opening Boss Style Gauge");
		gauge.SetDrawOrder(1400);
		gauge.SetPosition({ 580.0f, 58.0f });
		gauge.SetSize({ 760.0f, 22.0f });
		gauge.SetMaxValue(1.0f);
		gauge.SetValue(0.0f);
		gauge.SetBackgroundColor({ 0.025f, 0.018f, 0.025f, 0.94f });
		gauge.SetFillColor({ 0.94f, 0.64f, 0.12f, 0.98f });
		gauge.SetBorderColor({ 0.92f, 0.94f, 1.0f, 0.96f });
		gauge.SetBorderThickness(3.0f);
		gauge.SetVisible(false);
		gauge_ = &gauge;

		auto& title = AddComponent<K4E::TextComponent>();
		title.SetName("Stage 4 Opening Gauge Title");
		title.SetDrawOrder(1401);
		title.SetPosition({ 960.0f, 33.0f });
		title.SetAnchor({ 0.5f, 0.5f });
		title.SetFontName("DotGothic16");
		title.SetFontSize(27.0f);
		title.SetColor({ 0.96f, 0.98f, 1.0f, 1.0f });
		title.SetVisible(false);
		title_ = &title;

		SetName("Stage4OpeningGauge");
		SetLayer("StageUI");
		AddTag("Stage4OpeningGauge");
		K4E::Actor::Initialize();
	}

	void SetDisplay(bool visible, const std::string& text, float progress, const K4E::Vector4& fillColor)
	{
		if (gauge_)
		{
			gauge_->SetVisible(visible);
			gauge_->SetValue(std::clamp(progress, 0.0f, 1.0f));
			gauge_->SetFillColor(fillColor);
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

/// Stage 4序盤の装置起動、防衛、敵全滅、橋展開を一つの状態遷移として管理する。
class Stage4OpeningBridgeRuntime final
{
public:
	struct PromptSnapshot
	{
		bool visible = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const std::string& jsonPath = stageContext.GetCurrentStageAssets().jsonPath;
		active_ = jsonPath.find("fps_stage03.json") != std::string::npos;
	}

	void Finalize()
	{
		if (console_) console_->Destroy();
		if (bridge_) bridge_->Destroy();
		if (gaugeActor_) gaugeActor_->Destroy();
		console_ = nullptr;
		bridge_ = nullptr;
		gaugeActor_ = nullptr;
		prompt_ = {};
		state_ = State::WaitingForActivation;
		defenseElapsedSec_ = 0.0f;
		completionDisplayTimer_ = 0.0f;
		cameraMoveElapsedSec_ = 0.0f;
		cameraReturnElapsedSec_ = 0.0f;
		cameraShakeElapsedSec_ = 0.0f;
		savedCameraPosition_ = {};
		savedCameraForward_ = { 0.0f, 0.0f, 1.0f };
		cameraPresentationStarted_ = false;
		initialEncounterCount_ = 0;
		active_ = false;
	}

	void Update(
		IPlayerRuntime* player,
		K4E::Input* input,
		CharacterWorld& characters,
		float deltaTime)
	{
		prompt_ = {};
		if (!active_) return;

		SpawnActorsIfNeeded(characters.GetActorWorld());
		if (!console_ || !bridge_ || !gaugeActor_) return;

		const float safeDeltaTime = std::max(0.0f, deltaTime);
		switch (state_)
		{
		case State::WaitingForActivation:
			UpdateWaitingForActivation(player, input, characters, safeDeltaTime);
			break;
		case State::Defending:
			UpdateDefense(characters, safeDeltaTime);
			break;
		case State::CameraMoveToBridge:
			UpdateCameraMoveToBridge(player, safeDeltaTime);
			break;
		case State::OpeningBridge:
			UpdateBridgeOpening(player, safeDeltaTime);
			break;
		case State::CameraReturn:
			UpdateCameraReturn(player, safeDeltaTime);
			break;
		case State::Complete:
			UpdateComplete(safeDeltaTime);
			break;
		}
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (console_) console_->UpdateShadowMatrix(lightViewProjection);
	}

	bool IsActive() const { return active_; }
	bool IsComplete() const { return state_ == State::Complete; }
	const PromptSnapshot& GetPromptSnapshot() const { return prompt_; }

private:
	enum class State
	{
		WaitingForActivation,
		Defending,
		CameraMoveToBridge,
		OpeningBridge,
		CameraReturn,
		Complete,
	};

	static constexpr const char* kEncounterTag = "Stage4OpeningEncounterEnemy";

	void SpawnActorsIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (!bridge_)
		{
			auto& bridge = actorWorld.SpawnActor<Stage4CollapsedBridgeActor>();
			bridge.Update(0.0f);
			bridge_ = &bridge;
		}
		if (!console_)
		{
			auto& console = actorWorld.SpawnActor<Stage2DeviceActor>();
			console.Configure("CollapsedCity_BridgeControlSystem", { -12.0f, 0.0f, -59.0f });
			console.AddTag("Stage4OpeningDevice");
			console.Update(0.0f);
			console_ = &console;
		}
		if (!gaugeActor_)
		{
			auto& gauge = actorWorld.SpawnActor<Stage4OpeningGaugeActor>();
			gauge.SetDisplay(false, {}, 0.0f, { 0.94f, 0.64f, 0.12f, 0.98f });
			gaugeActor_ = &gauge;
		}
	}

	void UpdateWaitingForActivation(
		const IPlayerRuntime* player,
		K4E::Input* input,
		CharacterWorld& characters,
		float deltaTime)
	{
		const bool focused = player && console_->GetDistanceSquaredTo(player->GetWorldPosition()) <=
			console_->GetInteractionRadius() * console_->GetInteractionRadius();
		const bool interactHeld = focused && input && input->PushKey(DIK_E);
		const bool activatedNow = console_->UpdateInteraction(focused, interactHeld, deltaTime);
		console_->Update(0.0f);

		if (focused)
		{
			prompt_.visible = true;
			prompt_.text = console_->GetInteractionProgress() > 0.01f
				? "E 長押し：橋梁制御システム起動中"
				: "E 長押し：橋梁制御システムを起動";
			prompt_.normalizedProgress = console_->GetInteractionProgress();
			gaugeActor_->SetDisplay(
				true,
				"橋梁制御システム　起動準備",
				console_->GetInteractionProgress(),
				{ 0.22f, 0.72f, 1.0f, 0.98f });
		}
		else
		{
			gaugeActor_->SetDisplay(false, {}, 0.0f, { 0.22f, 0.72f, 1.0f, 0.98f });
		}

		if (!activatedNow) return;
		SpawnDefenseEncounter(characters);
		defenseElapsedSec_ = 0.0f;
		state_ = State::Defending;
	}

	void UpdateDefense(CharacterWorld& characters, float deltaTime)
	{
		defenseElapsedSec_ = std::min(defenseDurationSec_, defenseElapsedSec_ + deltaTime);
		const int aliveCount = CountAliveEncounterEnemies(characters);
		const bool minimumDefenseComplete = defenseElapsedSec_ >= defenseDurationSec_;

		char label[128]{};
		float progress = 0.0f;
		K4E::Vector4 fillColor{ 0.96f, 0.58f, 0.10f, 0.98f };
		if (!minimumDefenseComplete)
		{
			const float remaining = std::max(0.0f, defenseDurationSec_ - defenseElapsedSec_);
			std::snprintf(label, sizeof(label), "橋梁制御システム　防衛 %.1f秒", remaining);
			progress = defenseElapsedSec_ / std::max(0.05f, defenseDurationSec_);
		}
		else
		{
			std::snprintf(label, sizeof(label), "周辺の敵を排除　残り %d体", aliveCount);
			progress = initialEncounterCount_ > 0
				? 1.0f - static_cast<float>(aliveCount) / static_cast<float>(initialEncounterCount_)
				: 1.0f;
			fillColor = { 0.92f, 0.28f, 0.12f, 0.98f };
		}
		gaugeActor_->SetDisplay(true, label, progress, fillColor);

		if (!minimumDefenseComplete || aliveCount > 0) return;
		cameraMoveElapsedSec_ = 0.0f;
		cameraPresentationStarted_ = false;
		state_ = State::CameraMoveToBridge;
	}

	void UpdateCameraMoveToBridge(IPlayerRuntime* player, float deltaTime)
	{
		K4E::Camera* camera = player ? player->GetCamera() : nullptr;
		if (!camera)
		{
			bridge_->RequestOpen();
			state_ = State::OpeningBridge;
			return;
		}

		if (!cameraPresentationStarted_)
		{
			savedCameraPosition_ = camera->GetTranslate();
			savedCameraForward_ = NormalizeDirection(camera->GetForward(), { 0.0f, 0.0f, 1.0f });
			cameraPresentationStarted_ = true;
		}

		cameraMoveElapsedSec_ += deltaTime;
		const float t = SmoothStep(cameraMoveElapsedSec_ / std::max(0.05f, cameraMoveDurationSec_));
		const K4E::Vector3 startTarget = savedCameraPosition_ + savedCameraForward_ * 14.0f;
		ApplyCameraLookAt(
			camera,
			Lerp(savedCameraPosition_, bridgeCameraPosition_, t),
			Lerp(startTarget, bridgeCameraTarget_, t),
			{});
		gaugeActor_->SetDisplay(
			true,
			"崩落橋を展開準備",
			t,
			{ 0.18f, 0.72f, 0.94f, 0.98f });

		if (t < 1.0f) return;
		bridge_->RequestOpen();
		cameraShakeElapsedSec_ = 0.0f;
		state_ = State::OpeningBridge;
	}

	void UpdateBridgeOpening(IPlayerRuntime* player, float deltaTime)
	{
		const float progress = bridge_->GetOpenProgress();
		cameraShakeElapsedSec_ += deltaTime;
		if (K4E::Camera* camera = player ? player->GetCamera() : nullptr)
		{
			ApplyCameraLookAt(camera, bridgeCameraPosition_, bridgeCameraTarget_, BuildBridgeCameraShake(progress)); // 橋が組み上がっている間だけ専用カメラへ機械振動を加える。
		}
		gaugeActor_->SetDisplay(
			true,
			"崩落橋を展開中",
			progress,
			{ 0.18f, 0.72f, 0.94f, 0.98f });
		if (!bridge_->IsOpen()) return;

		cameraReturnElapsedSec_ = 0.0f;
		state_ = State::CameraReturn;
	}

	void UpdateCameraReturn(IPlayerRuntime* player, float deltaTime)
	{
		K4E::Camera* camera = player ? player->GetCamera() : nullptr;
		if (!camera)
		{
			completionDisplayTimer_ = 2.8f;
			state_ = State::Complete;
			return;
		}

		const K4E::Vector3 liveCameraPosition = camera->GetTranslate();
		const K4E::Vector3 liveCameraForward = NormalizeDirection(camera->GetForward(), savedCameraForward_);
		const K4E::Vector3 bridgeCameraForward = NormalizeDirection(bridgeCameraTarget_ - bridgeCameraPosition_, savedCameraForward_);
		cameraReturnElapsedSec_ += deltaTime;
		const float t = SmoothStep(cameraReturnElapsedSec_ / std::max(0.05f, cameraReturnDurationSec_));
		ApplyCameraForward(
			camera,
			Lerp(bridgeCameraPosition_, liveCameraPosition, t),
			NormalizeDirection(Lerp(bridgeCameraForward, liveCameraForward, t), liveCameraForward));
		gaugeActor_->SetDisplay(
			true,
			"橋梁ルート開通　先へ進め",
			1.0f,
			{ 0.22f, 0.94f, 0.42f, 0.98f });

		if (t < 1.0f) return;
		completionDisplayTimer_ = 2.8f;
		state_ = State::Complete;
	}

	void UpdateComplete(float deltaTime)
	{
		completionDisplayTimer_ = std::max(0.0f, completionDisplayTimer_ - deltaTime);
		const bool visible = completionDisplayTimer_ > 0.0f;
		gaugeActor_->SetDisplay(
			visible,
			"橋梁ルート開通　先へ進め",
			1.0f,
			{ 0.22f, 0.94f, 0.42f, 0.98f });
	}

	void SpawnDefenseEncounter(CharacterWorld& characters)
	{
		const std::array<K4E::Vector3, 5> positions = {{
			{ -17.0f, 2.0f, -69.0f },
			{ 17.0f, 2.0f, -69.0f },
			{ -20.0f, 2.0f, -55.0f },
			{ 20.0f, 2.0f, -55.0f },
			{ 0.0f, 2.0f, -80.0f }
		}};
		const std::array<EnemyType, 5> types = {{
			EnemyType::Melee,
			EnemyType::Melee,
			EnemyType::Melee,
			EnemyType::Melee,
			EnemyType::MidRange
		}};

		for (size_t index = 0; index < positions.size(); ++index)
		{
			EnemyBase& enemy = characters.SpawnEnemyAt(positions[index], types[index]);
			enemy.AddTag(kEncounterTag); // 専用Tagで他区画の敵と分け、序盤ギミックに必要な全滅数だけを数える。
		}
		initialEncounterCount_ = static_cast<int>(positions.size());
	}

	int CountAliveEncounterEnemies(const CharacterWorld& characters) const
	{
		int count = 0;
		for (EnemyBase* enemy : characters.GetEnemyRawList())
		{
			if (enemy && enemy->HasTag(kEncounterTag) && !enemy->IsDead()) ++count;
		}
		return count;
	}

	static float SmoothStep(float value)
	{
		const float clamped = std::clamp(value, 0.0f, 1.0f);
		return clamped * clamped * (3.0f - 2.0f * clamped);
	}

	static K4E::Vector3 Lerp(const K4E::Vector3& from, const K4E::Vector3& to, float t)
	{
		return from + (to - from) * std::clamp(t, 0.0f, 1.0f);
	}

	static K4E::Vector3 NormalizeDirection(const K4E::Vector3& direction, const K4E::Vector3& fallback)
	{
		const float lengthSq = K4E::Vector3::LengthSquared(direction);
		if (lengthSq <= 0.000001f) return fallback;
		return direction * (1.0f / std::sqrt(lengthSq));
	}

	void ApplyCameraLookAt(
		K4E::Camera* camera,
		const K4E::Vector3& cameraPosition,
		const K4E::Vector3& targetPosition,
		const K4E::Vector3& shakeOffset) const
	{
		if (!camera) return;
		const K4E::Vector3 shakenPosition = cameraPosition + shakeOffset;
		camera->SetTranslate(shakenPosition);
		camera->SetForward(NormalizeDirection(targetPosition - shakenPosition, savedCameraForward_));
		camera->Update();
	}

	void ApplyCameraForward(
		K4E::Camera* camera,
		const K4E::Vector3& cameraPosition,
		const K4E::Vector3& cameraForward) const
	{
		if (!camera) return;
		camera->SetTranslate(cameraPosition);
		camera->SetForward(NormalizeDirection(cameraForward, savedCameraForward_));
		camera->Update();
	}

	K4E::Vector3 BuildBridgeCameraShake(float progress) const
	{
		if (progress <= 0.0f || progress >= 1.0f) return {};
		const float fadeIn = SmoothStep(std::min(progress / 0.12f, 1.0f));
		const float fadeOut = SmoothStep(std::min((1.0f - progress) / 0.12f, 1.0f));
		const float amplitude = bridgeCameraShakeAmplitude_ * fadeIn * fadeOut;
		const float phase = cameraShakeElapsedSec_ * bridgeCameraShakeFrequency_;
		return {
			std::sin(phase * 1.37f) * amplitude,
			std::cos(phase * 1.91f) * amplitude * 0.55f,
			std::sin(phase * 0.73f) * amplitude * 0.35f
		};
	}

	Stage2DeviceActor* console_ = nullptr;
	Stage4CollapsedBridgeActor* bridge_ = nullptr;
	Stage4OpeningGaugeActor* gaugeActor_ = nullptr;
	PromptSnapshot prompt_{};
	State state_ = State::WaitingForActivation;
	K4E::Vector3 savedCameraPosition_{};
	K4E::Vector3 savedCameraForward_{ 0.0f, 0.0f, 1.0f };
	K4E::Vector3 bridgeCameraPosition_{ 10.5f, 7.5f, -62.0f };
	K4E::Vector3 bridgeCameraTarget_{ 0.0f, 0.55f, -29.0f };
	float defenseDurationSec_ = 8.0f;
	float defenseElapsedSec_ = 0.0f;
	float completionDisplayTimer_ = 0.0f;
	float cameraMoveDurationSec_ = 0.65f;
	float cameraReturnDurationSec_ = 0.75f;
	float cameraMoveElapsedSec_ = 0.0f;
	float cameraReturnElapsedSec_ = 0.0f;
	float cameraShakeElapsedSec_ = 0.0f;
	float bridgeCameraShakeAmplitude_ = 0.11f;
	float bridgeCameraShakeFrequency_ = 24.0f;
	int initialEncounterCount_ = 0;
	bool cameraPresentationStarted_ = false;
	bool active_ = false;
};
