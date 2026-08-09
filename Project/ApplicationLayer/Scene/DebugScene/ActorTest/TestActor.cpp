#include "TestActor.h"
#include "../Validation/PlayerMigrationValidationComponent.h"

#include <ActorJsonSerializer.h>
#include <CameraManager.h>
#include <Input.h>
#include <InputSnapshot.h>

namespace
{
	constexpr float kMouseLookSensitivity = 0.0025f;
	constexpr const char* kPlayerPrefabPath = "Resources/ActorPrefabs/Player.json";
}

void TestActor::Initialize()
{
	const bool hadSavedComposition = !GetComponents().empty();
	if (!hadSavedComposition)
	{
		Ken4lowEngine::ActorJsonSerializer::LoadActorFromFile(*this, kPlayerPrefabPath);
	}

	Ken4lowEngine::PlayerActor::Initialize();
	if (!GetComponent<Ken4lowEngine::PlayerMigrationValidationComponent>())
	{
		auto& validation = AddComponent<Ken4lowEngine::PlayerMigrationValidationComponent>();
		validation.SetName("Player P9 Validation");
		validation.SetUpdateOrder(200); // Player本体の各Component更新後にフレーム跨ぎの検証結果を判定する。
	}

	SetName("DebugPlayer"); // 共通PrefabのPlayer名をDebug専用保存先へ分離し、Player.jsonの誤上書きを防ぐ。
	wasControllingPlayer_ = false;
	if (!hadSavedComposition) ResetForValidation({ 0.0f, 1.5f, 0.0f }); // 共通Prefabの原点をDebugScene用検証位置へ移す。
}

void TestActor::Update(float deltaTime)
{
	auto* input = Ken4lowEngine::Input::GetInstance();
	auto* playerInput = GetPlayerInputComponent();
	auto* cameraManager = Ken4lowEngine::CameraManager::GetInstance();
	auto* validation = GetComponent<Ken4lowEngine::PlayerMigrationValidationComponent>();

	if (input && input->IsGameInputEnabled() && cameraManager->IsUsingDebugCamera()) cameraManager->SetUseDebugCamera(false);
	if (input && input->TriggerKey(DIK_F10) && validation) validation->RequestRunFullValidation();

	const bool useDebugCamera = cameraManager->IsUsingDebugCamera();
	const bool validationLocksInput = validation && validation->IsInputLocked();
	const bool canControlPlayer = input && playerInput && input->IsGameInputEnabled() && !useDebugCamera && !validationLocksInput;
	if (canControlPlayer)
	{
		const InputSnapshot inputSnapshot = Ken4lowEngine::BuildInputSnapshot(*input);
		playerInput->ApplyInputSnapshot(inputSnapshot, kMouseLookSensitivity, wasControllingPlayer_);
		wasControllingPlayer_ = true;

		if (input->TriggerKey(DIK_F6)) ApplyPlayerDamage(25.0f);
		if (input->TriggerKey(DIK_F7)) HealPlayer(25.0f);
		if (input->TriggerKey(DIK_F8))
		{
			const Ken4lowEngine::SceneComponent* root = GetRootComponent();
			ResetForValidation(root ? root->GetLocalPosition() : Ken4lowEngine::Vector3{}); // その場で新Player全機能を初期状態へ戻して再検証する。
		}
	}
	else
	{
		wasControllingPlayer_ = false;
		if (playerInput) playerInput->ResetInputState();
	}

	Ken4lowEngine::PlayerActor::Update(deltaTime);
}

void TestActor::PostPhysicsUpdate(float deltaTime)
{
	Ken4lowEngine::PlayerActor::PostPhysicsUpdate(deltaTime);
	if (!Ken4lowEngine::CameraManager::GetInstance()->IsUsingDebugCamera())
	{
		if (Ken4lowEngine::PlayerCameraComponent* camera = GetPlayerCameraComponent()) camera->SyncToMainCameraNow();
	}
}
