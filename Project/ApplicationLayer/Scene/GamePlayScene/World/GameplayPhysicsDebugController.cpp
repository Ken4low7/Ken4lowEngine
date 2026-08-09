#define NOMINMAX
#include "GameplayPhysicsDebugController.h"

#include "CharacterWorld.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

GameplayPhysicsDebugController::~GameplayPhysicsDebugController() = default;

void GameplayPhysicsDebugController::Initialize(const Dependencies& deps)
{
	deps_ = deps;
	// P13では旧Player専用の床判定/押し戻し比較経路を停止し、新Player本編PhysicsWorldとの二重登録を避ける。
}

void GameplayPhysicsDebugController::Finalize()
{
	deps_ = {};
}

void GameplayPhysicsDebugController::Update(const Dependencies& deps, float deltaTime)
{
	deps_ = deps;
	(void)deltaTime;
}

void GameplayPhysicsDebugController::Draw()
{
	// 新PlayerのPhysics可視化はCharacterWorld所有PhysicsWorld側へ再接続するまで本Controllerでは描画しない。
}

void GameplayPhysicsDebugController::DrawImGui(const Dependencies& deps)
{
	deps_ = deps;
#ifdef USE_IMGUI
	ImGui::SeparatorText("Gameplay Physics Migration");
	ImGui::TextUnformatted("P13: Legacy Player physics comparison is disabled.");
	ImGui::TextUnformatted("PlayerActor uses CharacterWorld-owned PhysicsWorld as the runtime source of truth.");
#else
	(void)deps;
#endif
}
