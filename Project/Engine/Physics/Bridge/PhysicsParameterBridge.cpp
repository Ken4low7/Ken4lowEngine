#include "PhysicsParameterBridge.h"

#include "PhysicsWorld.h"
#include "ParameterManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kPhysicsWorldGroup = "PhysicsWorld";
		constexpr const char* kPhysicsDebugDrawGroup = "PhysicsDebugDraw";
		constexpr const char* kGameplayPhysicsGroup = "GameplayPhysics";

		template<class T>
		T ReadValue(ParameterManager* parameters, const char* group, const char* key, const T& fallback)
		{
			try
			{
				return parameters->GetValue<T>(group, key);
			}
			catch (...)
			{
				return fallback;
			}
		}

		void RegisterWorldDefaults(ParameterManager* parameters)
		{
			parameters->CreateGroup(kPhysicsWorldGroup);
			parameters->AddItem(kPhysicsWorldGroup, "UseFixedStep", true);
			parameters->AddItem(kPhysicsWorldGroup, "FixedTimeStep", 1.0f / 60.0f, 1.0f / 240.0f, 1.0f / 15.0f);
			parameters->AddItem(kPhysicsWorldGroup, "MaxDeltaTime", 0.1f, 0.016f, 0.5f);
			parameters->AddItem(kPhysicsWorldGroup, "MaxSubSteps", static_cast<int32_t>(4), static_cast<int32_t>(1), static_cast<int32_t>(16));
			parameters->AddItem(kPhysicsWorldGroup, "GravityX", 0.0f, -100.0f, 100.0f);
			parameters->AddItem(kPhysicsWorldGroup, "GravityY", -9.8f, -100.0f, 100.0f);
			parameters->AddItem(kPhysicsWorldGroup, "GravityZ", 0.0f, -100.0f, 100.0f);
			parameters->AddItem(kPhysicsWorldGroup, "EnablePositionSolver", true);
			parameters->AddItem(kPhysicsWorldGroup, "EnableVelocitySolver", true);
			parameters->AddItem(kPhysicsWorldGroup, "EnableFrictionSolver", true);
			parameters->AddItem(kPhysicsWorldGroup, "EnableSleep", true);
		}

		void RegisterDebugDrawDefaults(ParameterManager* parameters)
		{
			parameters->CreateGroup(kPhysicsDebugDrawGroup);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawPhysicsDebug", false);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawColliders", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawContacts", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawContactNormals", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawVelocity", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawSleeping", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "DrawEvents", true);
			parameters->AddItem(kPhysicsDebugDrawGroup, "NormalLength", 1.5f, 0.0f, 10.0f);
			parameters->AddItem(kPhysicsDebugDrawGroup, "VelocityScale", 0.25f, 0.0f, 5.0f);
		}

		void RegisterGameplayDefaults(ParameterManager* parameters)
		{
			parameters->CreateGroup(kGameplayPhysicsGroup);
			parameters->AddItem(kGameplayPhysicsGroup, "EnableGameplayPhysicsTest", false);
			parameters->AddItem(kGameplayPhysicsGroup, "EnablePlayerPhysicsGroundCheck", false);
			parameters->AddItem(kGameplayPhysicsGroup, "EnablePlayerPhysicsDepenetration", false);
			parameters->AddItem(kGameplayPhysicsGroup, "ApplyPlayerPhysicsCorrectionXZ", true);
			parameters->AddItem(kGameplayPhysicsGroup, "ApplyPlayerPhysicsCorrectionY", false);
			parameters->AddItem(kGameplayPhysicsGroup, "PlayerCorrectionClamp", 1.0f, 0.0f, 5.0f);
			parameters->AddItem(kGameplayPhysicsGroup, "EnableGameplayPhysicsTriggerTest", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForPlayerStage", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForPlayerGround", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForPlayerDepenetration", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForTriggerTest", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForBulletTrigger", false);
			parameters->AddItem(kGameplayPhysicsGroup, "UsePhysicsForEnemyStage", false);
		}
	}

	void PhysicsParameterBridge::Initialize()
	{
		// ParameterManagerの値をPhysicsWorldへ橋渡しするため、保存対象の既定項目を登録する。
		ParameterManager* parameters = ParameterManager::GetInstance();
		RegisterWorldDefaults(parameters);
		RegisterDebugDrawDefaults(parameters);
		RegisterGameplayDefaults(parameters);
		Load();
		RefreshFromParameterManager();
		initialized_ = true;
	}

	void PhysicsParameterBridge::Finalize(const void* owner)
	{
		// 破棄済みインスタンスへの反映呼び出しを防ぐ。
		ParameterManager::GetInstance()->UnregisterParameterApplier(kPhysicsWorldGroup, owner);
		ParameterManager::GetInstance()->UnregisterParameterApplier(kPhysicsDebugDrawGroup, owner);
		ParameterManager::GetInstance()->UnregisterParameterApplier(kGameplayPhysicsGroup, owner);
	}

	void PhysicsParameterBridge::RegisterAppliers(const void* owner, std::function<void()> applyCallback)
	{
		if (!owner || !applyCallback)
		{
			return;
		}

		ParameterManager* parameters = ParameterManager::GetInstance();
		parameters->RegisterParameterApplier(kPhysicsWorldGroup, owner, applyCallback);
		parameters->RegisterParameterApplier(kPhysicsDebugDrawGroup, owner, applyCallback);
		parameters->RegisterParameterApplier(kGameplayPhysicsGroup, owner, applyCallback);
	}

	void PhysicsParameterBridge::ApplyTo(PhysicsWorld& physicsWorld)
	{
		// JSON/ImGuiで調整した値を物理ワールドへ反映する。
		RefreshFromParameterManager();
		physicsWorld.ApplySettings(worldSettings_);
	}

	void PhysicsParameterBridge::ApplyTo(PhysicsDebugDraw& debugDraw)
	{
		// JSON/ImGuiで調整したDebug表示設定をPhysicsDebugDrawへ反映する。
		RefreshFromParameterManager();
		debugDraw.SetSettings(debugDrawSettings_);
	}

	void PhysicsParameterBridge::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("Physics Parameter Bridge"))
		{
			ImGui::Text("Groups: PhysicsWorld / PhysicsDebugDraw / GameplayPhysics");
			if (ImGui::Button("Load Physics Parameters"))
			{
				Load();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Physics Parameters"))
			{
				Save();
			}
		}
#endif
	}

	void PhysicsParameterBridge::Save()
	{
		ParameterManager* parameters = ParameterManager::GetInstance();
		parameters->SaveFile(kPhysicsWorldGroup);
		parameters->SaveFile(kPhysicsDebugDrawGroup);
		parameters->SaveFile(kGameplayPhysicsGroup);
	}

	void PhysicsParameterBridge::Load()
	{
		ParameterManager* parameters = ParameterManager::GetInstance();
		parameters->LoadFile(kPhysicsWorldGroup);
		parameters->LoadFile(kPhysicsDebugDrawGroup);
		parameters->LoadFile(kGameplayPhysicsGroup);
		RefreshFromParameterManager();
	}

	PhysicsWorldSettings PhysicsParameterBridge::GetWorldSettings() const
	{
		return worldSettings_;
	}

	PhysicsDebugDrawSettings PhysicsParameterBridge::GetDebugDrawSettings() const
	{
		return debugDrawSettings_;
	}

	GameplayPhysicsSettings PhysicsParameterBridge::GetGameplaySettings() const
	{
		return gameplaySettings_;
	}

	void PhysicsParameterBridge::RefreshFromParameterManager()
	{
		ParameterManager* parameters = ParameterManager::GetInstance();

		worldSettings_.useFixedStep = ReadValue(parameters, kPhysicsWorldGroup, "UseFixedStep", worldSettings_.useFixedStep);
		worldSettings_.fixedTimeStep = std::clamp(ReadValue(parameters, kPhysicsWorldGroup, "FixedTimeStep", worldSettings_.fixedTimeStep), 1.0f / 240.0f, 1.0f / 15.0f);
		worldSettings_.maxDeltaTime = std::clamp(ReadValue(parameters, kPhysicsWorldGroup, "MaxDeltaTime", worldSettings_.maxDeltaTime), 0.016f, 0.5f);
		worldSettings_.maxSubSteps = std::clamp(ReadValue<int32_t>(parameters, kPhysicsWorldGroup, "MaxSubSteps", worldSettings_.maxSubSteps), 1, 16);
		worldSettings_.gravity.x = ReadValue(parameters, kPhysicsWorldGroup, "GravityX", worldSettings_.gravity.x);
		worldSettings_.gravity.y = ReadValue(parameters, kPhysicsWorldGroup, "GravityY", worldSettings_.gravity.y);
		worldSettings_.gravity.z = ReadValue(parameters, kPhysicsWorldGroup, "GravityZ", worldSettings_.gravity.z);
		worldSettings_.enablePositionSolver = ReadValue(parameters, kPhysicsWorldGroup, "EnablePositionSolver", worldSettings_.enablePositionSolver);
		worldSettings_.enableVelocitySolver = ReadValue(parameters, kPhysicsWorldGroup, "EnableVelocitySolver", worldSettings_.enableVelocitySolver);
		worldSettings_.enableFrictionSolver = ReadValue(parameters, kPhysicsWorldGroup, "EnableFrictionSolver", worldSettings_.enableFrictionSolver);
		worldSettings_.enableSleep = ReadValue(parameters, kPhysicsWorldGroup, "EnableSleep", worldSettings_.enableSleep);

		debugDrawSettings_.drawPhysicsDebug = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawPhysicsDebug", debugDrawSettings_.drawPhysicsDebug);
		debugDrawSettings_.drawColliders = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawColliders", debugDrawSettings_.drawColliders);
		debugDrawSettings_.drawContacts = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawContacts", debugDrawSettings_.drawContacts);
		debugDrawSettings_.drawContactNormals = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawContactNormals", debugDrawSettings_.drawContactNormals);
		debugDrawSettings_.drawVelocity = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawVelocity", debugDrawSettings_.drawVelocity);
		debugDrawSettings_.drawSleeping = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawSleeping", debugDrawSettings_.drawSleeping);
		debugDrawSettings_.drawEvents = ReadValue(parameters, kPhysicsDebugDrawGroup, "DrawEvents", debugDrawSettings_.drawEvents);
		debugDrawSettings_.normalLength = std::clamp(ReadValue(parameters, kPhysicsDebugDrawGroup, "NormalLength", debugDrawSettings_.normalLength), 0.0f, 10.0f);
		debugDrawSettings_.velocityScale = std::clamp(ReadValue(parameters, kPhysicsDebugDrawGroup, "VelocityScale", debugDrawSettings_.velocityScale), 0.0f, 5.0f);

		gameplaySettings_.enableGameplayPhysicsTest = ReadValue(parameters, kGameplayPhysicsGroup, "EnableGameplayPhysicsTest", gameplaySettings_.enableGameplayPhysicsTest);
		gameplaySettings_.enablePlayerPhysicsGroundCheck = ReadValue(parameters, kGameplayPhysicsGroup, "EnablePlayerPhysicsGroundCheck", gameplaySettings_.enablePlayerPhysicsGroundCheck);
		gameplaySettings_.enablePlayerPhysicsDepenetration = ReadValue(parameters, kGameplayPhysicsGroup, "EnablePlayerPhysicsDepenetration", gameplaySettings_.enablePlayerPhysicsDepenetration);
		gameplaySettings_.applyPlayerPhysicsCorrectionXZ = ReadValue(parameters, kGameplayPhysicsGroup, "ApplyPlayerPhysicsCorrectionXZ", gameplaySettings_.applyPlayerPhysicsCorrectionXZ);
		gameplaySettings_.applyPlayerPhysicsCorrectionY = ReadValue(parameters, kGameplayPhysicsGroup, "ApplyPlayerPhysicsCorrectionY", gameplaySettings_.applyPlayerPhysicsCorrectionY);
		gameplaySettings_.playerCorrectionClamp = std::clamp(ReadValue(parameters, kGameplayPhysicsGroup, "PlayerCorrectionClamp", gameplaySettings_.playerCorrectionClamp), 0.0f, 5.0f);
		gameplaySettings_.enableGameplayPhysicsTriggerTest = ReadValue(parameters, kGameplayPhysicsGroup, "EnableGameplayPhysicsTriggerTest", gameplaySettings_.enableGameplayPhysicsTriggerTest);
		gameplaySettings_.usePhysicsForPlayerStage = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForPlayerStage", gameplaySettings_.usePhysicsForPlayerStage);
		gameplaySettings_.usePhysicsForPlayerGround = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForPlayerGround", gameplaySettings_.usePhysicsForPlayerGround);
		gameplaySettings_.usePhysicsForPlayerDepenetration = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForPlayerDepenetration", gameplaySettings_.usePhysicsForPlayerDepenetration);
		gameplaySettings_.usePhysicsForTriggerTest = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForTriggerTest", gameplaySettings_.usePhysicsForTriggerTest);
		gameplaySettings_.usePhysicsForBulletTrigger = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForBulletTrigger", gameplaySettings_.usePhysicsForBulletTrigger);
		gameplaySettings_.usePhysicsForEnemyStage = ReadValue(parameters, kGameplayPhysicsGroup, "UsePhysicsForEnemyStage", gameplaySettings_.usePhysicsForEnemyStage);
	}

} // namespace Ken4lowEngine
