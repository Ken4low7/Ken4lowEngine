#define NOMINMAX
#include "BossBattleController.h"

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/DebugScene/DebugActorRegistration.h"
#include "Camera.h"
#include "CameraManager.h"
#include "CharacterWorld.h"
#include "CollisionManager.h"
#include "CollisionPreset.h"
#include "CollisionTypeIdDef.h"
#include "CrystalManager.h"
#include "GamePlayStageContext.h"
#include "HUDManager.h"
#include "Scene/Actor/Character/CharacterHealthComponent.h"
#include "Stage.h"
#include <LogString.h>
#include <TextComponent.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;
	constexpr float kBeginnerBossMaxHp = 900.0f;
	constexpr int kFinalBossStageIndex = 4;
	constexpr const char* kDefaultBossPrefabPath = "Resources/ActorPrefabs/ComponentBoss.json";
	constexpr const char* kMineCrusherBossPrefabPath = "Resources/ActorPrefabs/ComponentMineCrusherBoss.json";
}

void BossClearItem::Initialize(const K4E::Vector3& position)
{
	position_ = position + K4E::Vector3{ 0.0f, 1.25f, 0.0f };
	basePosition_ = position_;
	rotation_ = {};
	floatTimer_ = 0.0f;
	spawned_ = true;
	collected_ = false;
	ApplyCollisionPreset(*this, ECollisionPresetId::Item);
#ifdef _DEBUG
	assert(GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kItem));
#endif
	SetOwner<BossClearItem>(this);
	SetOBBHalfSize(halfSize_);
	SetCenterPosition(position_);
	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize("Sample/cube.gltf");
	object3d_->SetScale({ 1.8f, 1.8f, 1.8f });
	object3d_->SetTranslate(position_);
	object3d_->SetColor({ 1.0f, 0.85f, 0.05f, 1.0f });
	object3d_->Update();
}

void BossClearItem::Update(float deltaTime)
{
	if (!spawned_ || collected_) return;
	floatTimer_ += deltaTime * 3.0f;
	position_ = basePosition_;
	position_.y += std::sin(floatTimer_) * 0.25f;
	rotation_.y += deltaTime * 1.2f;
	SetCenterPosition(position_);
	SetOrientation(rotation_);
	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->SetRotate(rotation_);
		object3d_->Update();
	}
}

void BossClearItem::Draw(){ if (spawned_ && !collected_ && object3d_) object3d_->Draw(); }

bool BossClearItem::CheckPickup(const IPlayerRuntime& player) const
{
	return spawned_ && !collected_ && K4E::Vector3::Length(position_ - player.GetWorldPosition()) <= pickupRadius_;
}

void BossClearItem::MarkCollected()
{
	collected_ = true;
	SetEnabled(false);
	SetCenterPosition({ 1.0e9f, 1.0e9f, 1.0e9f });
}

void BossClearItem::OnCollision(K4E::Collider* other){ (void)other; }

void BossBattleController::Initialize(GamePlayStageContext& stageContext, bool beginnerBalance)
{
	stage1BeginnerBalanceEnabled_ = beginnerBalance;
	stage5Phase3Enabled_ = stageContext.GetCurrentStageIndex() == kFinalBossStageIndex; // 表示上のStage 5だけ第三フェーズを解禁する。
	const std::string& stageJsonPath = stageContext.GetCurrentStageAssets().jsonPath;
	bossPrefabPath_ = stageJsonPath.find("wasureraretakoudou") != std::string::npos
		? kMineCrusherBossPrefabPath
		: kDefaultBossPrefabPath; // Stage2だけ別Archetypeを選び、他ステージの既存Bossを維持する。
	bossSpawnPosition_ = stageContext.HasBossSpawnPoint() ? stageContext.GetBossSpawnPoint() : K4E::Vector3{ 0.0f, 2.25f, 30.0f };
	bossDeathPosition_ = bossSpawnPosition_;
	bossActor_ = nullptr;
	clearItem_.reset();
	manualBossRequest_ = false;
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossSpawnConditionMet_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	clearItemSpawned_ = false;
	clearItemCollected_ = false;
	isGameClear_ = false;
	cameraShakeTimer_ = cameraShakeDuration_ = cameraShakeAmplitude_ = cameraShakeFrequency_ = cameraShakeSeed_ = 0.0f;
	lastPresentedPhaseRevision_ = 0;
	bossIntroController_.Initialize(bossSpawnPosition_);
}

void BossBattleController::Finalize(const Dependencies& deps)
{
	DestroyBossActor(deps);
	if (clearItem_ && deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	clearItem_.reset();
	bossIntroController_.Finalize();
}

bool BossBattleController::RequestBossBattle(const K4E::Vector3& bossPosition)
{
	if (bossSpawned_ || bossIntroController_.HasPlayed() || bossIntroController_.IsRunning()) return false;
	bossSpawnPosition_ = bossPosition;
	bossDeathPosition_ = bossPosition;
	manualBossRequest_ = true;
	bossSpawnConditionMet_ = true;
	bossIntroController_.RequestStart(bossSpawnPosition_);
	return true;
}

void BossBattleController::UpdateSpawnProgress(const Dependencies& deps)
{
	const bool crystalCondition = deps.crystalManager && deps.crystalManager->AreAllCrystalsDestroyed();
	bossSpawnConditionMet_ = (manualBossRequest_ || crystalCondition) && !bossSpawned_;
	if (bossSpawnConditionMet_ && !bossIntroController_.HasPlayed() && !bossIntroController_.IsRunning()) bossIntroController_.RequestStart(bossSpawnPosition_);
}

void BossBattleController::UpdateIntro(const Dependencies& deps, float deltaTime)
{
	if (bossIntroController_.ConsumeDebugResetRequest()) ResetIntroForDebug(deps);
	if (bossIntroController_.ConsumeDebugStartRequest())
	{
		ResetIntroForDebug(deps);
		bossIntroController_.RequestStart(bossIntroController_.GetBossAppearPosition());
	}
	if (bossIntroController_.ConsumeDebugClearBossParentRequest() && bossActor_)
	{
		bossActor_->ClearRootParentKeepingWorldPosition();
		bossActor_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugForceBossToAppearRequest() && bossActor_)
	{
		bossActor_->ClearRootParentKeepingWorldPosition();
		bossActor_->SetPosition(bossIntroController_.GetBossAppearPosition());
		bossActor_->SetYaw(kPi);
		bossActor_->ForceSyncWorldTransform();
	}
	if (bossIntroController_.ConsumeDebugUseGameplayViewProjectionRequest())
	{
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && player->GetCamera())
		{
			K4E::CameraManager::GetInstance()->SetMainCamera(player->GetCamera());
			player->GetCamera()->Update();
		}
		if (bossActor_) bossActor_->ForceSyncWorldTransform();
	}

	if (!bossIntroController_.IsRunning()) return;
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (camera) K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	bossIntroController_.Update(deltaTime, bossActor_, camera);

	if (bossIntroController_.ConsumeBossSpawnRequest())
	{
		bossSpawnPosition_ = bossIntroController_.GetBossAppearPosition();
		SpawnBossActor(deps, false);
	}
	if (bossIntroController_.ConsumeBossColliderEnableRequest())
	{
		RegisterBossCollider(deps);
		if (player) AlignPlayerViewToBossAfterIntro(*player);
		if (deps.hudManager)
		{
			deps.hudManager->NotifyBossIntroCompleted(bossActor_ ? bossActor_->GetPosition() : bossSpawnPosition_);
			if (stage1BeginnerBalanceEnabled_) deps.hudManager->NotifyStage1BossAppeared();
		}
		StartCameraShake(0.75f, 0.42f, 24.0f);
	}
}

void BossBattleController::UpdateRuntime(const Dependencies& deps, float deltaTime)
{
	IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
	if (bossActor_)
	{
		if (deps.characters) bossActor_->SetTargetActor(deps.characters->GetPlayer());
		if (!bossDeathPositionCaptured_ && bossActor_->IsDead())
		{
			bossDeathPosition_ = bossActor_->GetDeathWorldPosition();
			bossDeathPositionCaptured_ = true;
		}
		HandleBossPhasePresentation(deps);
	}
	UpdateCameraShake(deltaTime, player);
	UpdateBossClearProgress(deps, deltaTime);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdatePausedWorld(const Dependencies& deps, float deltaTime)
{
	if (deps.crystalManager && deps.characters) deps.crystalManager->UpdatePresentationOnly(*deps.characters, deltaTime);
	UpdateIntro(deps, deltaTime);
	if (deps.stage) deps.stage->Update();
	UpdateBossClearProgress(deps, 0.0f);
	if (deps.crystalManager && deps.characters) deps.crystalManager->SetProgressDebugStatus(deps.characters->GetAliveNormalEnemyCount(), bossSpawnConditionMet_, bossSpawned_, bossSpawnPosition_);
}

void BossBattleController::UpdateHud(const Dependencies& deps, float deltaTime)
{
	(void)deltaTime;
	if (bossActor_)
	{
		bossActor_->SetHealthHudVisible(IsBossBattleActive());
		if (bossPrefabPath_ == kMineCrusherBossPrefabPath)
		{
			if (TextComponent* label = bossActor_->GetHealthLabelComponent()) label->SetText("坑道破砕体");
		}
	}
	if (deps.hudManager) deps.hudManager->SetBossHP(GetBossHP(), GetBossMaxHP(), false);
}

void BossBattleController::UpdateBossGuideHud(IPlayerRuntime& player, HUDManager& hudManager) const
{
	K4E::Camera* camera = player.GetCamera();
	const K4E::Vector3 position = bossActor_ ? bossActor_->GetPosition() : bossSpawnPosition_;
	hudManager.SetBossGuide(player.GetWorldPosition(), position, camera ? camera->GetForward() : K4E::Vector3{ 0.0f, 0.0f, 1.0f }, IsBossBattleActive());
}

void BossBattleController::DrawBoss()
{
	// 通常描画はCharacterWorldのActorWorld PassがBossActorを他Characterと一緒に描く。
}

void BossBattleController::DrawClearItem(){ if (clearItem_) clearItem_->Draw(); }
void BossBattleController::DrawBossIntro3D(){ if (bossActor_) { bossActor_->ForceSyncWorldTransform(); bossActor_->Draw(); } }
void BossBattleController::DrawShadow(){ /* 通常ShadowはCharacterWorldのActorWorld Passへ統一する。 */ }
void BossBattleController::DrawBossIntroShadow(){ if (bossActor_) bossActor_->DrawShadow(); }

void BossBattleController::DrawImGui(const Dependencies& deps, bool introPresentation)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("ボス状態");
	ImGui::Text("Prefab: %s", bossPrefabPath_.c_str());
	ImGui::Text("Phase 3: %s", stage5Phase3Enabled_ ? "Stage 5で有効" : "無効");
	ImGui::Text("ActorWorld Boss: %s / Collider: %s", bossSpawned_ ? "spawned" : "none", bossColliderRegistered_ ? "enabled" : "disabled");
	ImGui::Text("Intro: %s / Presentation camera: %s", bossIntroController_.IsRunning() ? "active" : "inactive", introPresentation ? "yes" : "no");
	bossIntroController_.SetDebugSnapshot(bossActor_, deps.characters && deps.characters->GetPlayerRuntime() ? deps.characters->GetPlayerRuntime()->GetCamera() : nullptr);
	bossIntroController_.DrawImGui();
	if (bossActor_)
	{
		ImGui::Text("HP: %.1f / %.1f / Phase: %d", bossActor_->GetHP(), bossActor_->GetMaxHP(), bossActor_->GetCurrentPhase());
		ImGui::Text("Battle: %s / Dead: %s / Death presentation: %s", bossActor_->IsBattleEnabled() ? "on" : "off", bossActor_->IsDead() ? "yes" : "no", bossActor_->IsDeathPresentationComplete() ? "complete" : "running");
	}
	ImGui::Text("Manual request: %s / Death position: %.2f %.2f %.2f / Clear item: %s", manualBossRequest_ ? "yes" : "no", bossDeathPosition_.x, bossDeathPosition_.y, bossDeathPosition_.z, clearItemSpawned_ ? "spawned" : "none");
#else
	(void)deps; (void)introPresentation;
#endif
}

void BossBattleController::ResetIntroForDebug(const Dependencies& deps)
{
	DestroyBossActor(deps);
	manualBossRequest_ = false;
	bossSpawned_ = false;
	bossColliderRegistered_ = false;
	bossDefeated_ = false;
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossSpawnConditionMet_ = false;
	lastPresentedPhaseRevision_ = 0;
	bossIntroController_.Reset();
}

bool BossBattleController::IsBossBattleActive() const
{
	return bossActor_ && bossColliderRegistered_ && bossActor_->IsAlive() && !bossIntroController_.IsGameplayPaused();
}

float BossBattleController::GetBossHP() const { return bossActor_ ? bossActor_->GetHP() : 0.0f; }
float BossBattleController::GetBossMaxHP() const { return bossActor_ ? bossActor_->GetMaxHP() : 0.0f; }

void BossBattleController::SpawnBossActor(const Dependencies& deps, bool enableBattleImmediately)
{
	if (bossSpawned_ || !deps.characters) return;
	RegisterApplicationActorTypes();
	K4E::ActorWorld& world = deps.characters->GetActorWorld();
	K4E::BossActor* actor = nullptr;
	if (K4E::Actor* prefabActor = world.SpawnActorFromJson(bossPrefabPath_))
	{
		actor = dynamic_cast<K4E::BossActor*>(prefabActor);
		if (!actor) world.DestroyActor(prefabActor);
	}
	if (!actor) actor = &world.SpawnActor<K4E::BossActor>();
	else actor->Initialize();

	bossActor_ = actor;
	const bool isMineCrusher = bossPrefabPath_ == kMineCrusherBossPrefabPath;
	bossActor_->SetName(isMineCrusher ? "GameplayMineCrusherBoss" : "GameplayBossActor");
	bossActor_->SetLayer("Boss");
	bossActor_->AddTag("Boss");
	bossActor_->AddTag("GameplayBoss");
	if (isMineCrusher) bossActor_->AddTag("MineCrusher");
	bossActor_->SetTargetActor(deps.characters->GetPlayer());
	if (auto* phase = bossActor_->GetBossPhaseComponent()) phase->SetPhase3Enabled(stage5Phase3Enabled_);
	if (stage1BeginnerBalanceEnabled_)
	{
		if (auto* health = bossActor_->GetHealthComponent()) health->ResetHealth(kBeginnerBossMaxHp);
	}
	bossDeathPositionCaptured_ = false;
	bossDeathPosition_ = bossSpawnPosition_;
	bossActor_->SetPosition(enableBattleImmediately ? bossSpawnPosition_ : bossIntroController_.GetBossStartPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(enableBattleImmediately);
	bossActor_->SetHealthHudVisible(enableBattleImmediately);
	bossActor_->ForceSyncWorldTransform();
	bossSpawned_ = true;
	lastPresentedPhaseRevision_ = bossActor_->GetPhaseRevision();
	if (enableBattleImmediately) RegisterBossCollider(deps);
}

void BossBattleController::RegisterBossCollider(const Dependencies& deps)
{
	if (!bossActor_ || bossColliderRegistered_) return;
	bossActor_->ClearRootParentKeepingWorldPosition();
	bossActor_->SetPosition(bossIntroController_.GetBossAppearPosition());
	bossActor_->SetYaw(kPi);
	bossActor_->SetBattleEnabled(true);
	bossActor_->SetHealthHudVisible(true);
	bossActor_->ForceSyncWorldTransform();
	if (deps.collisionManager && bossActor_->GetCollisionPrimitive()) deps.collisionManager->AddCollider(bossActor_->GetCollisionPrimitive());
	bossColliderRegistered_ = true;
	K4E::Log("[BossActor] Legacy query collider registered.\n");
}

void BossBattleController::DestroyBossActor(const Dependencies& deps)
{
	if (!bossActor_) return;
	if (deps.collisionManager && bossColliderRegistered_ && bossActor_->GetCollisionPrimitive()) deps.collisionManager->RemoveCollider(bossActor_->GetCollisionPrimitive());
	bossActor_->SetBattleEnabled(false);
	bossActor_->SetHealthHudVisible(false);
	bossActor_->SetActive(false);
	if (deps.characters) deps.characters->GetActorWorld().DestroyActor(bossActor_);
	bossActor_ = nullptr;
	bossColliderRegistered_ = false;
}

void BossBattleController::AlignPlayerViewToBossAfterIntro(IPlayerRuntime& player) const
{
	K4E::Camera* camera = player.GetCamera();
	if (!camera) return;
	float pitch = 0.0f;
	float yaw = 0.0f;
	if (CalcLookAnglesToTarget(camera->GetTranslate(), bossIntroController_.GetBossLookTarget(), pitch, yaw)) player.SetViewLookAngles(pitch, yaw);
	K4E::CameraManager::GetInstance()->SetMainCamera(camera);
	camera->Update();
}

void BossBattleController::UpdateBossClearProgress(const Dependencies& deps, float deltaTime)
{
	if (bossActor_ && bossActor_->IsDead() && !bossDefeated_)
	{
		bossDefeated_ = true;
		if (!bossDeathPositionCaptured_)
		{
			bossDeathPosition_ = bossActor_->GetDeathWorldPosition();
			bossDeathPositionCaptured_ = true;
		}
		if (deps.setBossDefeated) deps.setBossDefeated(false);
	}
	if (bossDefeated_ && bossActor_ && bossActor_->IsDeathPresentationComplete() && !clearItemSpawned_ && bossDeathPositionCaptured_) SpawnClearItem(deps, bossDeathPosition_);
	if (clearItem_ && !clearItemCollected_)
	{
		clearItem_->Update(deltaTime);
		IPlayerRuntime* player = deps.characters ? deps.characters->GetPlayerRuntime() : nullptr;
		if (player && clearItem_->CheckPickup(*player)) CollectClearItem(deps);
	}
}

void BossBattleController::SpawnClearItem(const Dependencies& deps, const K4E::Vector3& deathPosition)
{
	if (clearItemSpawned_) return;
	K4E::Vector3 position = deathPosition;
	if (deps.stage)
	{
		float floorY = -std::numeric_limits<float>::infinity();
		float nearest = std::numeric_limits<float>::infinity();
		for (const K4E::AABB& floor : deps.stage->GetFloorAABBs())
		{
			if (position.x < floor.min.x || position.x > floor.max.x || position.z < floor.min.z || position.z > floor.max.z) continue;
			const float distance = std::abs(floor.max.y - position.y);
			if (distance < nearest){ nearest = distance; floorY = floor.max.y; }
		}
		if (std::isfinite(floorY)) position.y = floorY;
	}
	position.y = std::max(position.y, 0.0f);
	clearItem_ = std::make_unique<BossClearItem>();
	clearItem_->Initialize(position);
	if (deps.collisionManager) deps.collisionManager->AddCollider(clearItem_.get());
	clearItemSpawned_ = true;
	K4E::Log("[GameClear] BossClearItem spawned at BossActor death position.\n");
}

void BossBattleController::CollectClearItem(const Dependencies& deps)
{
	if (clearItemCollected_ || isGameClear_) return;
	clearItemCollected_ = true;
	isGameClear_ = true;
	if (clearItem_)
	{
		clearItem_->MarkCollected();
		if (deps.collisionManager) deps.collisionManager->RemoveCollider(clearItem_.get());
	}
	if (deps.setBossDefeated) deps.setBossDefeated(true);
}

void BossBattleController::HandleBossPhasePresentation(const Dependencies& deps)
{
	(void)deps;
	if (!bossActor_) return;
	const unsigned int revision = bossActor_->GetPhaseRevision();
	if (revision == lastPresentedPhaseRevision_) return;
	lastPresentedPhaseRevision_ = revision;
	const int phase = bossActor_->GetCurrentPhase();
	if (phase >= 3) StartCameraShake(0.85f, 0.40f, 25.0f);
	else if (phase >= 2) StartCameraShake(0.55f, 0.24f, 20.0f);
}

void BossBattleController::StartCameraShake(float duration, float amplitude, float frequency)
{
	if (duration <= 0.0f || amplitude <= 0.0f) return;
	cameraShakeDuration_ = cameraShakeTimer_ = duration;
	cameraShakeAmplitude_ = amplitude;
	cameraShakeFrequency_ = std::max(1.0f, frequency);
	cameraShakeSeed_ += 2.31f;
}

void BossBattleController::UpdateCameraShake(float deltaTime, IPlayerRuntime* player)
{
	if (cameraShakeTimer_ <= 0.0f) return;
	cameraShakeTimer_ = std::max(0.0f, cameraShakeTimer_ - deltaTime);
	K4E::Camera* camera = player ? player->GetCamera() : nullptr;
	if (!camera) return;
	camera->SetTranslate(camera->GetTranslate() + BuildCameraShakeOffset());
	camera->Update();
}

K4E::Vector3 BossBattleController::BuildCameraShakeOffset() const
{
	if (cameraShakeTimer_ <= 0.0f || cameraShakeDuration_ <= 0.0f) return {};
	const float rate = std::clamp(cameraShakeTimer_ / cameraShakeDuration_, 0.0f, 1.0f);
	const float t = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
	const float amplitude = cameraShakeAmplitude_ * rate * rate;
	return { std::sin(t * 1.51f) * amplitude, std::cos(t * 1.13f) * amplitude * 0.50f, std::sin(t * 0.83f) * amplitude * 0.30f };
}

bool BossBattleController::CalcLookAnglesToTarget(const K4E::Vector3& from, const K4E::Vector3& target, float& pitch, float& yaw)
{
	K4E::Vector3 direction = target - from;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) return false;
	direction = K4E::Vector3::Normalize(direction);
	yaw = std::atan2(-direction.x, direction.z);
	pitch = std::atan2(-direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
	return true;
}
