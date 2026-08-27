#define NOMINMAX
#include "DebugScene.h"
#include <Input.h>
#include <SpriteManager.h>
#include "CameraManager.h"
#include "Wireframe.h"
#include <GameTimer.h>

#include "DebugActorRegistration.h"
#include "TestGroundActor.h"

// テスト用
#include "ActorTest/BasicParticleActor.h"

#include <ActorJsonSerializer.h>
#include <ColliderComponent.h>
#include <RigidbodyComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

#ifdef USE_IMGUI
#include <Editor/EditorPlayController.h>
#include <Editor/EditorPlaySessionManager.h>
#include <imgui.h>
#endif // USE_IMGUI

using namespace Ken4lowEngine;

namespace
{
	constexpr K4E::SystemResourceId kWorldObjectState = 1;
	constexpr K4E::SystemResourceId kLocalTransformState = 2;
	constexpr K4E::SystemResourceId kWorldTransformState = 3;
	constexpr K4E::SystemResourceId kPhysicsRegistry = 4;
	constexpr K4E::SystemResourceId kPhysicsState = 5;
	constexpr K4E::SystemResourceId kRenderState = 6;
}

DebugScene::DebugScene() = default;
DebugScene::~DebugScene() = default;

/// -------------------------------------------------------------
///							初期化処理
/// -------------------------------------------------------------
void DebugScene::Initialize()
{
	RegisterDebugActors(); // DebugScene専用のActorを登録する

	input_ = Input::GetInstance();

	actorWorld_.SetPhysicsWorld(&actorPhysicsWorld_);
	actorPhysicsWorld_.SetUseFixedStep(false);

	/*TestGroundActor& validationGround = actorWorld_.SpawnActor<TestGroundActor>();
	validationGround.SetName("ValidationGround");
	validationGround.SetLayer("DebugValidation");*/

	// テスト用にBasicParticleActorを生成する
	auto& particleTest = actorWorld_.SpawnActor<BasicParticleActor>();

	// 基礎VFX確認用Actorとして識別しやすい名前を設定する
	particleTest.SetName("BasicParticleTest");


	actorWorld_.Initialize();
	SetupWorldSystemSchedule();
}

void DebugScene::BeginEditorPlay()
{
#ifdef USE_IMGUI
	K4E::EditorPlayController::GetInstance()->CaptureGameInput(); // Play開始時からMain ViewportをFPS操作へ切り替える。
#endif
	if (input_)
	{
		input_->SetLockCursor(true);
		input_->SetCursorVisible(false);
	}
}

void DebugScene::EndEditorPlay()
{
#ifdef USE_IMGUI
	K4E::EditorPlayController::GetInstance()->ReleaseGameInput(); // Stop後はEditorへ入力を返し、カーソル固定を解除する。
#endif
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

/// -------------------------------------------------------------
///							更新処理
/// -------------------------------------------------------------
void DebugScene::Update()
{
#ifdef _DEBUG
	UpdateDebug();
#endif // _DEBUG

	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();
	performancePhaseValidation_.BeginFrame(deltaTime);
	ProcessActorWorldValidationRequests();

	worldSystemScheduler_.ExecuteAndWait(deltaTime); // World phase間の待機はSystemSchedulerがread/write dependencyから解決する。

	performancePhaseValidation_.SetPhysicsState(
		actorWorld_.GetActors().size(),
		actorPhysicsWorld_.GetColliderCount(),
		actorPhysicsWorld_.GetContactCount(),
		actorPhysicsWorld_.GetLastSubStepCount());
}

void DebugScene::SetupWorldSystemSchedule()
{
	using Access = K4E::SystemResourceAccess;
	using AccessType = K4E::SystemAccessType;
	using Policy = K4E::SystemExecutionPolicy;

	worldSystemScheduler_.Reset();
	worldSystemScheduler_.AddSystem(
		"ActorWorld.Update",
		[this](float deltaTime)
		{
			performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::ActorWorldUpdate);
			actorWorld_.Update(deltaTime);
			performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::ActorWorldUpdate);
		},
		std::vector<Access>{
			{ kWorldObjectState, AccessType::ReadWrite },
			{ kLocalTransformState, AccessType::ReadWrite },
			{ kWorldTransformState, AccessType::ReadWrite },
			{ kPhysicsRegistry, AccessType::ReadWrite },
			{ kRenderState, AccessType::Write },
		},
		Policy::MainThread);

	worldSystemScheduler_.AddSystem(
		"ActorWorld.FinalizePrePhysicsTransforms",
		[this](float)
		{
			prePhysicsTransformStats_ = FinalizeDirtyWorldTransforms();
		},
		std::vector<Access>{
			{ kLocalTransformState, AccessType::Read },
			{ kWorldTransformState, AccessType::Write },
		},
		Policy::MainThread); // Physicsへ渡す直前にdirtyなWorldTransformだけを確定する。

	worldSystemScheduler_.AddSystem(
		"PhysicsWorld.Update",
		[this](float deltaTime)
		{
			performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::PhysicsWorldUpdate);
			actorPhysicsWorld_.Update(deltaTime);
			performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::PhysicsWorldUpdate);
		},
		std::vector<Access>{
			{ kWorldObjectState, AccessType::ReadWrite },
			{ kWorldTransformState, AccessType::Read },
			{ kPhysicsRegistry, AccessType::Read },
			{ kPhysicsState, AccessType::ReadWrite },
		},
		Policy::MainThread); // Physics event callbackはActor/Componentへ戻るためMainThread affinityを維持する。

	worldSystemScheduler_.AddSystem(
		"ActorWorld.PostPhysicsUpdate",
		[this](float deltaTime)
		{
			performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::PostPhysicsUpdate);
			actorWorld_.PostPhysicsUpdate(deltaTime);
			performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::PostPhysicsUpdate);
		},
		std::vector<Access>{
			{ kWorldObjectState, AccessType::ReadWrite },
			{ kPhysicsState, AccessType::Read },
			{ kLocalTransformState, AccessType::ReadWrite },
			{ kWorldTransformState, AccessType::ReadWrite },
			{ kRenderState, AccessType::Write },
		},
		Policy::MainThread);

	worldSystemScheduler_.AddSystem(
		"ActorWorld.FinalizePostPhysicsTransforms",
		[this](float)
		{
			postPhysicsTransformStats_ = FinalizeDirtyWorldTransforms();
		},
		std::vector<Access>{
			{ kLocalTransformState, AccessType::Read },
			{ kWorldTransformState, AccessType::Write },
			{ kRenderState, AccessType::Write },
		},
		Policy::MainThread); // 物理補正後に残ったdirtyだけを描画前の確定状態へ反映する。

	if (!worldSystemScheduler_.Compile())
	{
		throw std::logic_error("DebugScene world system schedule contains a dependency cycle.");
	}
}

DebugScene::TransformFinalizeStats DebugScene::FinalizeDirtyWorldTransforms()
{
	TransformFinalizeStats stats{};
	std::vector<K4E::SceneComponent*> sceneComponents;
	for (const auto& actor : actorWorld_.GetActors())
	{
		if (!actor || actor->IsPendingDestroy() || !actor->IsActive()) continue;
		for (const auto& component : actor->GetComponents())
		{
			auto* sceneComponent = dynamic_cast<K4E::SceneComponent*>(component.get());
			if (!sceneComponent || !sceneComponent->IsActiveInHierarchy()) continue;
			sceneComponents.push_back(sceneComponent);
			if (!sceneComponent->GetParent()) ++stats.rootCount;
			if (sceneComponent->IsWorldTransformDirty()) ++stats.dirtyComponentCount;
		}
	}

	// Dirty数を先にsnapshotしてからflushするため、Rootが子を先に更新しても診断値が過少にならない。
	for (K4E::SceneComponent* sceneComponent : sceneComponents)
	{
		stats.recomputedComponentCount += sceneComponent->RefreshWorldTransformHierarchy();
	}
	return stats;
}

void DebugScene::UpdateEditor(float deltaTime)
{
	(void)deltaTime;
	ProcessActorWorldValidationRequests(); // Edit/Pause中の操作要求も次のActorWorld::UpdateEditor前に処理する。
}

/// -------------------------------------------------------------
///							3D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw3DObjects()
{
	performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::ActorDraw);
	actorWorld_.Draw();
	performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::ActorDraw);

	// ActorComponent由来のColliderをWireframe表示する
	performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::PhysicsDebugDraw);
	actorPhysicsDebugDraw_.Draw(actorPhysicsWorld_);
	performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::PhysicsDebugDraw);

#ifdef _DEBUG
	// ワイヤーフレームの描画
	Wireframe::GetInstance()->DrawGrid(100.0f, 50.0f, { 0.25f, 0.25f, 0.25f, 1.0f });
#endif // _DEBUG
}

/// -------------------------------------------------------------
///					シャドウマップ描画処理
/// -------------------------------------------------------------
void DebugScene::DrawShadowObjects()
{
	performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::ShadowDraw);
	actorWorld_.DrawShadow();
	performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::ShadowDraw);
}

/// -------------------------------------------------------------
///							2D描画処理
/// -------------------------------------------------------------
void DebugScene::Draw2DSprites()
{
#pragma region スプライトの描画
	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();
#pragma endregion

#pragma region UIの描画
	// Actorに追加されたScreen Space Spriteを3D描画後にまとめて描画する
	performancePhaseValidation_.BeginPhase(PerformancePhaseValidation::Phase::ScreenSpaceUI);
	actorWorld_.DrawScreenSpaceUI();
	performancePhaseValidation_.EndPhase(PerformancePhaseValidation::Phase::ScreenSpaceUI);
#pragma endregion
}

/// -------------------------------------------------------------
///							終了処理
/// -------------------------------------------------------------
void DebugScene::Finalize()
{
	// 入力状態を必ず戻す（ロック/非表示のまま終了しない）
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}

	worldSystemScheduler_.Reset();
	prePhysicsTransformStats_ = {};
	postPhysicsTransformStats_ = {};
	actorWorld_.Finalize();
	input_ = nullptr;
}

/// -------------------------------------------------------------
///							ImGui描画処理
/// -------------------------------------------------------------
void DebugScene::DrawImGui()
{
#ifdef USE_IMGUI
	// ActorWorldのEditor UIはEngine/Editor側のBridgeを正規導線とし、Runtime Worldから直接描画しない。
	DrawActorWorldValidationImGui();
	performancePhaseValidation_.DrawImGui(); // 実フレームとDebugScene各処理区間のCPU時間を比較する。
	// 旧LevelData/Import互換UIは削除し、現在のActorWorldと性能診断だけを表示する。

	actorWorld_.DrawImGui();
	actorPhysicsDebugDraw_.DrawImGui(actorPhysicsWorld_); // Debug描画はユーザー設定を尊重し、毎フレーム強制ONしない。
#endif // USE_IMGUI
}

K4E::Actor* DebugScene::FindActorWorldValidationTarget() const
{
	for (const auto& actor : actorWorld_.GetActors())
	{
		if (actor && actor->GetName() == actorWorldValidation_.targetActorName) return actor.get();
	}
	return nullptr;
}

void DebugScene::ProcessActorWorldValidationRequests()
{
	auto& validation = actorWorldValidation_;
	K4E::Actor* target = FindActorWorldValidationTarget();

	if (validation.requestSpawn)
	{
		validation.requestSpawn = false;
		if (!target)
		{
			validation.lastMessage = "DebugPlayerをActorWorld経由で生成しました。";
			validation.lastSucceeded = true;
		}
	}

	if (validation.requestToggleActive)
	{
		validation.requestToggleActive = false;
		if (target)
		{
			target->SetActive(!target->IsActive());
			validation.lastMessage = target->IsActive() ? "Actorを有効化しました。" : "Actorを無効化しました。";
			validation.lastSucceeded = true;
		}
	}

	if (validation.requestSave)
	{
		validation.requestSave = false;
		validation.lastSucceeded = target && actorWorld_.SaveActorToJson(*target, validation.jsonPath);
		validation.lastMessage = validation.lastSucceeded ? "Actor JSONを保存しました。" : "Actor JSONの保存に失敗しました。";
	}

	if (validation.requestReload)
	{
		validation.requestReload = false;
		validation.lastSucceeded = target && actorWorld_.ReloadActorFromJson(*target, validation.jsonPath);
		validation.lastMessage = validation.lastSucceeded ? "Actor JSONを安全な経路で再読込しました。" : "Actor JSONの再読込に失敗しました。";
	}

	if (validation.requestSpawnFromJson)
	{
		validation.requestSpawnFromJson = false;
		if (target && !actorWorld_.SaveActorToJson(*target, validation.jsonPath))
		{
			validation.lastSucceeded = false;
			validation.lastMessage = "複製用Actor JSONの保存に失敗しました。";
		}
		else
		{
			validation.lastSucceeded = actorWorld_.SpawnActorFromJson(validation.jsonPath) != nullptr;
			validation.lastMessage = validation.lastSucceeded ? "JSONからActorを複製生成しました。" : "JSONからのActor生成に失敗しました。";
		}
	}

	if (validation.requestDestroy)
	{
		validation.requestDestroy = false;
		validation.lastSucceeded = actorWorld_.DestroyActor(target);
		validation.pendingDestroyCheck = validation.lastSucceeded;
		validation.lastMessage = validation.lastSucceeded ? "Actorの遅延削除を予約しました。" : "削除対象Actorが見つかりません。";
	}
}

void DebugScene::RunActorWorldValidation()
{
	auto& validation = actorWorldValidation_;
	K4E::Actor* target = FindActorWorldValidationTarget();
	if (!target)
	{
		validation.lastSucceeded = false;
		validation.lastMessage = "DebugPlayerが存在しません。生成ボタンから復元できます。";
		return;
	}

	bool succeeded = target->GetRootComponent() != nullptr && !target->GetComponents().empty();
	int previousUpdateOrder = (std::numeric_limits<int>::min)();
	std::vector<const K4E::ActorComponent*> orderedComponents;
	for (const auto& component : target->GetComponents())
	{
		if (!component || !component->IsInitialized()) succeeded = false;
		if (component) orderedComponents.push_back(component.get());
	}
	std::stable_sort(orderedComponents.begin(), orderedComponents.end(),
		[](const K4E::ActorComponent* lhs, const K4E::ActorComponent* rhs)
		{
			return lhs->GetUpdateOrder() < rhs->GetUpdateOrder();
		});
	for (const K4E::ActorComponent* component : orderedComponents)
	{
		if (component->GetUpdateOrder() < previousUpdateOrder) succeeded = false;
		previousUpdateOrder = component->GetUpdateOrder();
	}

	const nlohmann::json snapshot = K4E::ActorJsonSerializer::SerializeActor(*target);
	succeeded = succeeded && snapshot.contains("Active") && snapshot.contains("Components") && snapshot["Components"].is_array();

	std::size_t expectedColliderCount = 0;
	std::size_t expectedRigidbodyCount = 0;
	for (const auto& actor : actorWorld_.GetActors())
	{
		if (!actor || !actor->IsActive() || actor->IsPendingDestroy()) continue;
		for (const K4E::ColliderComponent* collider : actor->GetComponents<K4E::ColliderComponent>())
		{
			if (collider && collider->IsActiveInHierarchy() && collider->GetCollider()) ++expectedColliderCount;
		}
		const K4E::RigidbodyComponent* rigidbody = actor->GetComponent<K4E::RigidbodyComponent>();
		if (rigidbody && rigidbody->IsActiveInHierarchy() && rigidbody->GetRigidbody()) ++expectedRigidbodyCount;
	}
	succeeded = succeeded && actorPhysicsWorld_.GetColliderCount() == expectedColliderCount;
	succeeded = succeeded && actorPhysicsWorld_.GetRigidbodies().size() == expectedRigidbodyCount;

	std::ostringstream message;
	message << (succeeded ? "ActorWorldの自動検証に成功しました。" : "ActorWorldの自動検証で不整合を検出しました。")
		<< " Actor=" << actorWorld_.GetActors().size()
		<< " Collider=" << actorPhysicsWorld_.GetColliderCount()
		<< " Rigidbody=" << actorPhysicsWorld_.GetRigidbodies().size();
	validation.lastSucceeded = succeeded;
	validation.lastMessage = message.str();
}

void DebugScene::DrawActorWorldValidationImGui()
{
#ifdef USE_IMGUI
	auto& validation = actorWorldValidation_;
	if (validation.pendingDestroyCheck)
	{
		validation.pendingDestroyCheck = false;
		validation.lastSucceeded = FindActorWorldValidationTarget() == nullptr;
		validation.lastMessage = validation.lastSucceeded
			? "遅延削除、Physics解除、内部参照解除を確認しました。"
			: "削除予約したActorがまだ残っています。";
	}
	if (validation.requestValidation)
	{
		validation.requestValidation = false;
		RunActorWorldValidation();
	}

	if (!ImGui::Begin("ActorWorld 検証"))
	{
		ImGui::End();
		return;
	}

	K4E::Actor* target = FindActorWorldValidationTarget();
	ImGui::Text("実行状態: %s / %s",
		K4E::EditorPlayController::GetInstance()->GetPlayStateText(),
		K4E::EditorPlaySessionManager::GetInstance()->GetWorldDomainText());
	ImGui::Text("操作Player: %s", target ? target->GetName().c_str() : "なし");
	ImGui::Text("Actor数: %zu", actorWorld_.GetActors().size());
	ImGui::Text("Collider登録数: %zu", actorPhysicsWorld_.GetColliderCount());
	ImGui::Text("Rigidbody登録数: %zu", actorPhysicsWorld_.GetRigidbodies().size());
	const K4E::SystemScheduleStats& scheduleStats = worldSystemScheduler_.GetStats();
	ImGui::Text("World Schedule: System=%zu / Dependency=%zu / Main=%zu / Worker=%zu",
		scheduleStats.systemCount,
		scheduleStats.dependencyCount,
		scheduleStats.mainThreadSystemCount,
		scheduleStats.workerSystemCount);
	ImGui::Text("Dirty Transform PrePhysics: Dirty=%zu / Recomputed=%zu / Roots=%zu",
		prePhysicsTransformStats_.dirtyComponentCount,
		prePhysicsTransformStats_.recomputedComponentCount,
		prePhysicsTransformStats_.rootCount);
	ImGui::Text("Dirty Transform PostPhysics: Dirty=%zu / Recomputed=%zu / Roots=%zu",
		postPhysicsTransformStats_.dirtyComponentCount,
		postPhysicsTransformStats_.recomputedComponentCount,
		postPhysicsTransformStats_.rootCount);
	ImGui::TextColored(validation.lastSucceeded ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
		"%s", validation.lastMessage.c_str());
	if (ImGui::Button("自動検証")) validation.requestValidation = true;
	ImGui::SameLine();
	if (ImGui::Button("DebugPlayer生成")) validation.requestSpawn = true;
	ImGui::SameLine();
	if (ImGui::Button("Active切替")) validation.requestToggleActive = true;

	if (ImGui::Button("JSON保存")) validation.requestSave = true;
	ImGui::SameLine();
	if (ImGui::Button("JSON再読込")) validation.requestReload = true;
	ImGui::SameLine();
	if (ImGui::Button("JSONから複製")) validation.requestSpawnFromJson = true;

	if (ImGui::Button("遅延削除テスト")) validation.requestDestroy = true;
	ImGui::TextDisabled("Play中はDebugPlayerをEnemy/Bossの唯一のTargetとして使用します。");
	ImGui::End();
#endif
}

/// -------------------------------------------------------------
///						Debug用の更新処理
/// -------------------------------------------------------------
void DebugScene::UpdateDebug()
{
	// Inputがない場合は何もしない（安全策）
	if (!input_) return;

	// F9はEditor操作中でも使いたいDebugショートカットなのでRaw入力で判定する
	if (input_->TriggerRawKey(DIK_F9))
	{
		// デバッグカメラの使用状態をトグルで切り替える
		const bool nextDebugCamera = !CameraManager::GetInstance()->IsUsingDebugCamera();

		// 切り替えた状態をCameraManagerとWireframeに伝える
		CameraManager::GetInstance()->SetUseDebugCamera(nextDebugCamera);
		Wireframe::GetInstance()->SetDebugCamera(nextDebugCamera);

		isDebugCamera_ = nextDebugCamera;
	}
}