#pragma once

#include "ApplicationLayer/Character/CharacterWorld/CharacterWorld.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"
#include "Stage2DeviceActor.h"
#include "Stage2HiddenPassageActor.h"
#include "Stage3DefenseRuntime.h"
#include "Stage4AthleticRuntime.h"
#include "Stage4OpeningBridgeRuntime.h"

#include <ActorWorld.h>
#include <Input.h>
#include <LightManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

/// Stage 2装置、Stage 3防衛対象、Stage 4の序盤・中盤ギミックを既存CharacterWorldのActorWorldへ生成し、Objective通知を管理する。
class Stage2CharacterDeviceManager final
{
public:
	struct PromptSnapshot
	{
		bool visible = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

	static constexpr const char* GetBossArenaEnteredEventId() { return "__Stage2BossArenaEntered"; }
	static constexpr const char* GetDefenseTargetDestroyedEventId() { return Stage3DefenseRuntime::GetDestroyedEventId(); }

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		defenseRuntime_.Initialize(stageContext);
		if (defenseRuntime_.IsActive()) return;
		openingBridgeRuntime_.Initialize(stageContext);
		athleticRuntime_.Initialize(stageContext);
		if (openingBridgeRuntime_.IsActive() || athleticRuntime_.IsActive()) return; // Stage 4では橋と崩落高架を同時に初期化し、Stage 2装置処理とは分離する。

		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::ActivateDevices;
		if (!active_) return;
		pendingDevicePoints_ = stageContext.GetDevicePoints();
		if (stageContext.GetCurrentStageAssets().jsonPath.find("wasureraretakoudou") != std::string::npos)
		{
			pendingDevicePoints_ = {
				{ "MineDevice_West", { -34.0f, 1.2f, -10.0f } },
				{ "MineDevice_East", { 38.0f, 2.0f, 45.0f } },
				{ "MineDevice_Deep", { 0.0f, 2.0f, 110.0f } }
			}; // 各装置を西作業床・東採掘床・最奥制御床の実際の床面Yへ配置する。
			hiddenPassageEnabled_ = true;
			ConfigureMineLighting();
		}
		requiredDeviceCount_ = std::max(1, rule.requiredDeviceCount);
	}

	void Finalize()
	{
		defenseRuntime_.Finalize();
		openingBridgeRuntime_.Finalize();
		athleticRuntime_.Finalize();
		for (Stage2DeviceActor* device : devices_)
		{
			if (device) device->Destroy();
		}
		if (hiddenGate_) hiddenGate_->Destroy();
		devices_.clear();
		hiddenGate_ = nullptr;
		pendingDevicePoints_.clear();
		prompt_ = {};
		encounterTriggered_.fill(false);
		requiredDeviceCount_ = 0;
		activationSequence_ = 0;
		feedbackTimer_ = 0.0f;
		bossArenaEnterRequest_ = false;
		bossArenaEntered_ = false;
		bossPhaseStarted_ = false;
		hiddenPassageEnabled_ = false;
		active_ = false;
	}

	void Update(
		IPlayerRuntime* player,
		K4E::Input* input,
		CharacterWorld& characters,
		float deltaTime,
		const std::function<void(const std::string&)>& onActivated)
	{
		prompt_ = {};
		if (defenseRuntime_.IsActive())
		{
			defenseRuntime_.Update(player, characters, deltaTime, onActivated);
			const auto& defensePrompt = defenseRuntime_.GetPromptSnapshot();
			prompt_.visible = defensePrompt.visible;
			prompt_.text = defensePrompt.text;
			prompt_.normalizedProgress = defensePrompt.normalizedProgress;
			return; // Stage 3は装置操作を行わず、防衛コアと増援Runtimeだけを更新する。
		}
		if (openingBridgeRuntime_.IsActive() || athleticRuntime_.IsActive())
		{
			if (openingBridgeRuntime_.IsActive())
			{
				openingBridgeRuntime_.Update(player, input, characters, deltaTime);
				const auto& openingPrompt = openingBridgeRuntime_.GetPromptSnapshot();
				prompt_.visible = openingPrompt.visible;
				prompt_.text = openingPrompt.text;
				prompt_.normalizedProgress = openingPrompt.normalizedProgress;
			}
			if (athleticRuntime_.IsActive())
			{
				athleticRuntime_.Update(player, characters.GetActorWorld(), deltaTime);
			}
			return; // Stage 4は序盤の操作案内と中盤の崩落UIを、それぞれの専用Runtimeで並行更新する。
		}
		if (!active_) return;
		K4E::ActorWorld& actorWorld = characters.GetActorWorld();
		SpawnDevicesIfNeeded(actorWorld);
		SpawnHiddenGateIfNeeded(actorWorld);
		UpdateAmbientEncounters(player, characters);

		feedbackTimer_ = std::max(0.0f, feedbackTimer_ - std::max(0.0f, deltaTime));
		Stage2DeviceActor* nearest = FindNearestDevice(player);
		const bool interactHeld = nearest && input && input->PushKey(DIK_E);

		for (Stage2DeviceActor* device : devices_)
		{
			if (!device) continue;
			const bool activatedNow = device->UpdateInteraction(device == nearest, device == nearest && interactHeld, deltaTime);
			device->Update(0.0f);
			if (!activatedNow) continue;

			++activationSequence_;
			feedbackTimer_ = activationSequence_ >= requiredDeviceCount_ ? 2.0f : 1.1f;
			if (activationSequence_ < requiredDeviceCount_)
			{
				SpawnActivationReinforcements(*device, characters);
			}
			else if (hiddenGate_)
			{
				hiddenGate_->RequestOpen(); // 3基目ではBossを即出現させず、封鎖壁を開いて探索区間へつなぐ。
			}
			if (onActivated) onActivated(device->GetDeviceId());
		}

		if (AreAllDevicesActivated() && hiddenGate_ && !hiddenGate_->IsOpen()) hiddenGate_->RequestOpen();
		UpdateBossArenaEntry(player);
		if (bossArenaEnterRequest_ && onActivated)
		{
			onActivated(GetBossArenaEnteredEventId());
			bossArenaEnterRequest_ = false; // 既存Device通知経路を再利用し、World側に別Callback引数を増やさない。
		}
		BuildPrompt(nearest);
	}

	void Draw() {}
	void DrawShadow() {}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		defenseRuntime_.UpdateShadowMatrix(lightViewProjection);
		openingBridgeRuntime_.UpdateShadowMatrix(lightViewProjection);
		for (Stage2DeviceActor* device : devices_)
		{
			if (device) device->UpdateShadowMatrix(lightViewProjection);
		}
		if (hiddenGate_) hiddenGate_->UpdateShadowMatrix(lightViewProjection);
	}

	const PromptSnapshot& GetPromptSnapshot() const { return prompt_; }
	int GetActivatedCount() const
	{
		return static_cast<int>(std::count_if(devices_.begin(), devices_.end(), [](const Stage2DeviceActor* device)
			{
				return device && device->IsActivated();
			}));
	}
	bool AreAllDevicesActivated() const { return active_ && requiredDeviceCount_ > 0 && GetActivatedCount() >= requiredDeviceCount_; }
	bool IsHiddenPassageOpen() const { return hiddenGate_ && hiddenGate_->IsOpen(); }
	bool HasEnteredBossArena() const { return bossArenaEntered_; }
	bool IsActive() const { return active_ || defenseRuntime_.IsActive() || openingBridgeRuntime_.IsActive() || athleticRuntime_.IsActive(); }
	bool IsDefenseActive() const { return defenseRuntime_.IsActive(); }
	const K4E::Vector3& GetBossArenaPosition() const { return bossArenaPosition_; }
	Stage3DefenseTargetActor* GetDefenseTarget() const { return defenseRuntime_.GetTarget(); }

private:
	static void ConfigureMineLighting()
	{
		auto* lightManager = K4E::LightManager::GetInstance();
		if (!lightManager) return;
		lightManager->ResetToDefaultLighting();

		auto& settings = lightManager->GetMutableLightingSettingsForEditor();
		settings.ambientColor = { 0.018f, 0.022f, 0.030f, 0.10f };
		settings.fogColor = { 0.030f, 0.040f, 0.050f, 1.0f };
		settings.exposure = 0.92f;
		settings.contrast = 1.12f;
		settings.fogStart = 34.0f;
		settings.fogEnd = 175.0f;
		settings.enableFog = 1u;
		settings.specularStrength = 0.07f;
		settings.diffuseStrength = 0.88f;
		settings.enableHalfLambert = 1u;

		auto& lights = lightManager->GetMutablePunctualLightsForEditor();
		lights.clear();
		lights.reserve(32);

		K4E::LightManager::PunctualLightGPU directional{};
		directional.lightType = 1u;
		directional.color = { 0.42f, 0.48f, 0.58f, 1.0f };
		directional.intensity = 0.16f;
		directional.direction = { 0.25f, -0.94f, 0.22f };
		directional.enabled = 1u;
		lights.push_back(directional);

		auto addPoint = [&lights](const K4E::Vector3& position, const K4E::Vector4& color, float intensity, float radius)
		{
			K4E::LightManager::PunctualLightGPU light{};
			light.lightType = 2u;
			light.color = color;
			light.intensity = intensity;
			light.position = position;
			light.radius = radius;
			light.decay = 1.65f;
			light.enabled = 1u;
			lights.push_back(light);
		};

		const std::array<K4E::Vector3, 13> workLights = {{
			{ 0.0f, 5.8f, -44.0f }, { 0.0f, 5.8f, -22.0f }, { -34.0f, 6.2f, -10.0f },
			{ 0.0f, 5.8f, 4.0f }, { 0.0f, 5.8f, 26.0f }, { 34.0f, 7.0f, 45.0f },
			{ 0.0f, 6.2f, 52.0f }, { 0.0f, 6.5f, 76.0f }, { -22.0f, 7.0f, 92.0f },
			{ 22.0f, 7.0f, 102.0f }, { 0.0f, 7.0f, 112.0f }, { -40.0f, 6.8f, 58.0f },
			{ 40.0f, 6.8f, 80.0f }
		}};
		for (size_t index = 0; index < workLights.size(); ++index)
		{
			const float variation = static_cast<float>(index % 3) * 0.08f;
			addPoint(workLights[index], { 1.0f, 0.58f + variation, 0.28f, 1.0f }, 1.05f, 17.0f);
		}

		addPoint({ 0.0f, 5.0f, 128.0f }, { 0.20f, 0.58f, 1.0f, 1.0f }, 1.15f, 14.0f);
		addPoint({ 0.0f, 5.0f, 140.0f }, { 0.18f, 0.52f, 1.0f, 1.0f }, 1.25f, 14.0f);
		addPoint({ 0.0f, 5.5f, 152.0f }, { 0.25f, 0.68f, 1.0f, 1.0f }, 1.35f, 16.0f);

		const K4E::Vector4 arenaWarm{ 1.0f, 0.48f, 0.18f, 1.0f };
		addPoint({ -19.0f, 7.5f, 168.0f }, arenaWarm, 1.55f, 21.0f);
		addPoint({ 19.0f, 7.5f, 168.0f }, arenaWarm, 1.55f, 21.0f);
		addPoint({ -19.0f, 7.5f, 192.0f }, arenaWarm, 1.55f, 21.0f);
		addPoint({ 19.0f, 7.5f, 192.0f }, arenaWarm, 1.55f, 21.0f);

		K4E::LightManager::PunctualLightGPU arenaSpot{};
		arenaSpot.lightType = 3u;
		arenaSpot.color = { 0.54f, 0.72f, 1.0f, 1.0f };
		arenaSpot.intensity = 2.15f;
		arenaSpot.position = { 0.0f, 12.0f, 180.0f };
		arenaSpot.direction = { 0.0f, -1.0f, 0.0f };
		arenaSpot.distance = 34.0f;
		arenaSpot.decay = 1.35f;
		arenaSpot.cosFalloffStart = 0.82f;
		arenaSpot.cosAngle = 0.55f;
		arenaSpot.enabled = 1u;
		lights.push_back(arenaSpot);

		lightManager->SetShadowCasterLightIndex(0);
		lightManager->SetManualShadowFocusPosition({ 0.0f, 2.0f, 110.0f });
		lightManager->SetDirectionalShadowFrustum(120.0f, 120.0f, 0.1f, 280.0f); // 低い環境光と局所灯を併用し、暗さを保ちながら進路と敵の輪郭を残す。
	}

	void SpawnDevicesIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (!devices_.empty() || pendingDevicePoints_.empty()) return;
		const int spawnCount = std::min(requiredDeviceCount_, static_cast<int>(pendingDevicePoints_.size()));
		for (int index = 0; index < spawnCount; ++index)
		{
			auto& actor = actorWorld.SpawnActor<Stage2DeviceActor>();
			const auto& point = pendingDevicePoints_[static_cast<size_t>(index)];
			const std::string id = point.name.empty() ? "Stage2Device_" + std::to_string(index + 1) : point.name;
			actor.Configure(id, point.position);
			actor.Update(0.0f);
			devices_.push_back(&actor);
		}
	}

	void SpawnHiddenGateIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (!hiddenPassageEnabled_ || hiddenGate_) return;
		auto& gate = actorWorld.SpawnActor<Stage2HiddenPassageActor>();
		gate.Configure({ 0.0f, 6.0f, 119.0f }, 10.5f);
		gate.Update(0.0f);
		hiddenGate_ = &gate;
	}

	Stage2DeviceActor* FindNearestDevice(const IPlayerRuntime* player) const
	{
		if (!player) return nullptr;
		const K4E::Vector3 playerPosition = player->GetWorldPosition();
		Stage2DeviceActor* nearest = nullptr;
		float nearestDistanceSq = 1.0e30f;
		for (Stage2DeviceActor* device : devices_)
		{
			if (!device || device->IsActivated()) continue;
			const float distanceSq = device->GetDistanceSquaredTo(playerPosition);
			const float radius = device->GetInteractionRadius();
			if (distanceSq <= radius * radius && distanceSq < nearestDistanceSq)
			{
				nearest = device;
				nearestDistanceSq = distanceSq;
			}
		}
		return nearest;
	}

	void UpdateBossArenaEntry(const IPlayerRuntime* player)
	{
		if (!player || bossArenaEntered_ || !AreAllDevicesActivated() || !IsHiddenPassageOpen()) return;
		const K4E::Vector3 position = player->GetWorldPosition();
		const bool insideEntrance = std::abs(position.x) <= 22.0f && position.z >= 158.0f && position.z <= 202.0f;
		if (!insideEntrance) return;
		bossArenaEntered_ = true;
		bossPhaseStarted_ = true;
		bossArenaEnterRequest_ = true; // 広間へ踏み込んだ瞬間だけBossIntroへ要求し、通路内ではカメラを奪わない。
		feedbackTimer_ = 1.8f;
	}

	void BuildPrompt(const Stage2DeviceActor* nearest)
	{
		if (bossArenaEntered_)
		{
			if (feedbackTimer_ > 0.0f)
			{
				prompt_.visible = true;
				prompt_.text = "大広間に巨大な反応を検知";
				prompt_.normalizedProgress = 1.0f;
			}
			return;
		}
		if (AreAllDevicesActivated())
		{
			prompt_.visible = true;
			if (hiddenGate_ && !hiddenGate_->IsOpen())
			{
				prompt_.text = "封鎖壁を開放中";
				prompt_.normalizedProgress = hiddenGate_->GetOpenProgress();
			}
			else
			{
				prompt_.text = "隠し通路が開いた　最奥の広間へ進め";
				prompt_.normalizedProgress = 1.0f;
			}
			return;
		}
		if (feedbackTimer_ > 0.0f)
		{
			prompt_.visible = true;
			prompt_.text = "装置を起動しました";
			prompt_.normalizedProgress = 1.0f;
		}
		else if (nearest && !nearest->IsActivated())
		{
			prompt_.visible = true;
			prompt_.text = nearest->GetInteractionProgress() > 0.01f ? "E 長押し：起動中" : "E 長押し：装置を起動";
			prompt_.normalizedProgress = nearest->GetInteractionProgress();
		}
	}

	void UpdateAmbientEncounters(const IPlayerRuntime* player, CharacterWorld& characters)
	{
		if (!player || AreAllDevicesActivated() || bossPhaseStarted_ || characters.GetAliveNormalEnemyCount() > 9) return;
		const K4E::Vector3 playerPosition = player->GetWorldPosition();
		const std::array<K4E::Vector3, 5> centers = {{
			{ 0.0f, 0.0f, -31.0f },
			{ -34.0f, 1.2f, -10.0f },
			{ 0.0f, 0.0f, 22.0f },
			{ 36.0f, 2.0f, 45.0f },
			{ 0.0f, 1.5f, 82.0f }
		}};
		const std::array<float, 5> radii = { 18.0f, 16.0f, 18.0f, 16.0f, 20.0f };

		for (size_t index = 0; index < centers.size(); ++index)
		{
			if (encounterTriggered_[index]) continue;
			const K4E::Vector3 delta = playerPosition - centers[index];
			const float distanceSq = delta.x * delta.x + delta.z * delta.z;
			if (distanceSq > radii[index] * radii[index]) continue;
			SpawnAmbientEncounter(index, characters);
			encounterTriggered_[index] = true;
			break;
		}
	}

	void SpawnAmbientEncounter(size_t index, CharacterWorld& characters)
	{
		switch (index)
		{
		case 0:
			characters.SpawnEnemyAt({ -7.0f, 2.0f, -25.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 7.0f, 2.0f, -20.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 0.0f, 2.0f, -14.0f }, EnemyType::MidRange);
			break;
		case 1:
			characters.SpawnEnemyAt({ -43.0f, 3.2f, -17.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -27.0f, 3.2f, -16.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -42.0f, 3.2f, 1.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -25.0f, 3.2f, 2.0f }, EnemyType::MidRange);
			break;
		case 2:
			characters.SpawnEnemyAt({ -8.0f, 2.0f, 18.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 8.0f, 2.0f, 24.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -5.0f, 2.0f, 32.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 6.0f, 2.0f, 36.0f }, EnemyType::MidRange);
			break;
		case 3:
			characters.SpawnEnemyAt({ 27.0f, 4.0f, 37.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 43.0f, 4.0f, 39.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 29.0f, 4.0f, 55.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 43.0f, 4.0f, 54.0f }, EnemyType::MidRange);
			break;
		case 4:
		default:
			characters.SpawnEnemyAt({ -21.0f, 3.5f, 82.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 21.0f, 3.5f, 82.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -12.0f, 4.0f, 94.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ 12.0f, 4.0f, 96.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt({ -20.0f, 4.0f, 103.0f }, EnemyType::MidRange);
			characters.SpawnEnemyAt({ 20.0f, 4.0f, 104.0f }, EnemyType::MidRange);
			break;
		}
	}

	void SpawnActivationReinforcements(const Stage2DeviceActor& device, CharacterWorld& characters)
	{
		const K4E::Vector3 center = device.GetPosition();
		characters.SpawnEnemyAt(center + K4E::Vector3{ 6.0f, 2.0f, 2.0f }, EnemyType::Melee);
		characters.SpawnEnemyAt(center + K4E::Vector3{ -6.0f, 2.0f, -3.0f }, EnemyType::Melee);
		characters.SpawnEnemyAt(center + K4E::Vector3{ 1.5f, 2.0f, 7.0f }, EnemyType::MidRange);
		if (activationSequence_ >= 2)
		{
			characters.SpawnEnemyAt(center + K4E::Vector3{ -2.0f, 2.0f, 8.0f }, EnemyType::Melee);
			characters.SpawnEnemyAt(center + K4E::Vector3{ 8.0f, 2.0f, -5.0f }, EnemyType::MidRange); // 増援は装置床面を基準に高さを加え、段差内部への埋没を防ぐ。
		}
	}

	Stage3DefenseRuntime defenseRuntime_{};
	Stage4OpeningBridgeRuntime openingBridgeRuntime_{};
	Stage4AthleticRuntime athleticRuntime_{};
	std::vector<Stage2DeviceActor*> devices_;
	Stage2HiddenPassageActor* hiddenGate_ = nullptr;
	std::vector<GamePlayStageContext::DevicePointInfo> pendingDevicePoints_;
	std::array<bool, 5> encounterTriggered_{};
	PromptSnapshot prompt_{};
	K4E::Vector3 bossArenaPosition_{ 0.0f, 4.35f, 180.0f };
	int requiredDeviceCount_ = 0;
	int activationSequence_ = 0;
	float feedbackTimer_ = 0.0f;
	bool bossArenaEnterRequest_ = false;
	bool bossArenaEntered_ = false;
	bool bossPhaseStarted_ = false;
	bool hiddenPassageEnabled_ = false;
	bool active_ = false;
};
