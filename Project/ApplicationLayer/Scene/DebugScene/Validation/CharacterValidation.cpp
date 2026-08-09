#include "CharacterValidation.h"

#include <ActorJsonSerializer.h>
#include <ActorWorld.h>
#include <SceneComponent.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/CharacterTargetComponent.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <sstream>
#include <string_view>

#ifdef USE_IMGUI
#include <Editor/EditorPlayController.h>
#include <imgui.h>
#endif

namespace
{
	constexpr float kValidationEpsilon = 0.0001f;

	/// 2つのVector3が検証用許容誤差内で一致するか判定する。
	bool NearlyEqual(const K4E::Vector3& lhs, const K4E::Vector3& rhs)
	{
		return std::abs(lhs.x - rhs.x) <= kValidationEpsilon &&
			std::abs(lhs.y - rhs.y) <= kValidationEpsilon &&
			std::abs(lhs.z - rhs.z) <= kValidationEpsilon;
	}

	/// Vector3の全成分が有限値か判定する。
	bool IsFinite(const K4E::Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

#ifdef USE_IMGUI
	/// 人型表示の各検証結果をOKまたはNG付きで1行表示する。
	void DrawValidationResult(const char* label, bool succeeded)
	{
		ImGui::TextColored(succeeded ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
			"%s: %s", label, succeeded ? "OK" : "NG");
	}
#endif
}

bool CharacterValidation::HumanoidValidationResults::AllSucceeded() const
{
	return composition && models && actorTransform && partHierarchy && partVisibility && shadow &&
		colliderFollow && definitionJson && actorJson && editorAndPlay;
}

void CharacterValidation::Initialize(K4E::ActorWorld& actorWorld)
{
	actorWorld_ = &actorWorld;
	if (K4E::CharacterActor* existingCharacter = FindTarget())
	{
		ConfigureTarget(*existingCharacter);
		return; // Scene再初期化時も同名の検証Actorを重複生成しない。
	}
	K4E::CharacterActor& character = actorWorld_->SpawnActor<K4E::CharacterActor>();
	ConfigureTarget(character);
}

void CharacterValidation::ProcessRequests()
{
	if (!actorWorld_) return;
	K4E::CharacterActor* target = FindTarget();

	if (requestSpawn_)
	{
		requestSpawn_ = false;
		if (!target)
		{
			listenerActor_ = nullptr;
			listenerId_ = 0;
			K4E::CharacterActor& actor = actorWorld_->SpawnActor<K4E::CharacterActor>();
			ConfigureTarget(actor);
			target = &actor;
			lastSucceeded_ = true;
			lastMessage_ = "検証用キャラクターを生成しました。";
		}
	}

	if (requestValidation_)
	{
		requestValidation_ = false;
		RunValidation(); // GPU描画開始前の更新フェーズで人型再構築と一時Actor破棄を完結させる。
	}

	if (requestReloadDefinition_)
	{
		requestReloadDefinition_ = false;
		K4E::HumanoidVisualComponent* visual = FindHumanoidVisual();
		std::string error;
		lastSucceeded_ = visual && visual->LoadDefinitionFromFile(visual->GetDefinitionPath(), &error);
		lastMessage_ = lastSucceeded_ ? "人型定義をJSONから再読み込みしました。" : "人型定義の再読み込みに失敗しました: " + error;
	}

	if (requestToggleHead_)
	{
		requestToggleHead_ = false;
		TogglePartVisibility("Head");
	}
	if (requestToggleLeftArm_)
	{
		requestToggleLeftArm_ = false;
		TogglePartVisibility("LeftArm");
	}
	if (requestToggleRightArm_)
	{
		requestToggleRightArm_ = false;
		TogglePartVisibility("RightArm");
	}
	if (requestShowAllParts_)
	{
		requestShowAllParts_ = false;
		K4E::HumanoidVisualComponent* visual = FindHumanoidVisual();
		lastSucceeded_ = visual != nullptr;
		if (visual) visual->SetAllPartsVisible(true);
		lastMessage_ = lastSucceeded_ ? "全部位を表示しました。" : "人型表示Componentが見つかりません。";
	}

	if (requestDamage_)
	{
		requestDamage_ = false;
		const K4E::CharacterDamageResult result = target ? target->ApplyDamage(25.0f) : K4E::CharacterDamageResult{};
		lastAppliedDamage_ = result.appliedDamage;
		lastSucceeded_ = result.accepted;
		lastMessage_ = result.accepted ? "HPへ25ダメージを適用しました。" : "ダメージを適用できませんでした。";
	}

	if (requestLethalDamage_)
	{
		requestLethalDamage_ = false;
		const float lethalDamage = target && target->GetHealthComponent()
			? target->GetHealthComponent()->GetCurrentHealth() + 1.0f
			: 0.0f;
		const K4E::CharacterDamageResult result = target ? target->ApplyDamage(lethalDamage) : K4E::CharacterDamageResult{};
		lastAppliedDamage_ = result.appliedDamage;
		lastSucceeded_ = result.killed;
		lastMessage_ = result.killed ? "死亡遷移と死亡イベントを通知しました。" : "致死ダメージを適用できませんでした。";
	}

	if (requestRestore_)
	{
		requestRestore_ = false;
		K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
		if (health)
		{
			health->RestoreFullHealth();
			lastSucceeded_ = target->IsAlive();
			lastMessage_ = "HPを全回復しました。";
		}
	}

	if (requestMove_)
	{
		requestMove_ = false;
		K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
		if (movement)
		{
			movement->SetMovementEnabled(true);
			movement->SetVelocity({ 1.0f, 0.0f, 0.0f });
			lastSucceeded_ = true;
			lastMessage_ = "移動速度を設定しました。";
		}
	}

	if (requestStop_)
	{
		requestStop_ = false;
		K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
		if (movement)
		{
			movement->Stop();
			lastSucceeded_ = true;
			lastMessage_ = "移動を停止しました。";
		}
	}

	if (requestSave_)
	{
		requestSave_ = false;
		lastSucceeded_ = target && actorWorld_->SaveActorToJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "キャラクター設定をJSONへ保存しました。" : "キャラクター設定の保存に失敗しました。";
	}

	if (requestReload_)
	{
		requestReload_ = false;
		lastSucceeded_ = target && actorWorld_->ReloadActorFromJson(*target, jsonPath_);
		lastMessage_ = lastSucceeded_ ? "共通機能と設定値をJSONから復元しました。" : "キャラクター設定の再読込に失敗しました。";
	}
}

void CharacterValidation::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::Begin("キャラクター共通機能 検証"))
	{
		ImGui::End();
		return;
	}

	K4E::CharacterActor* target = FindTarget();
	const K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	const K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	const K4E::CharacterColliderComponent* collider = target ? target->GetColliderComponent() : nullptr;
	const K4E::CharacterAnimationComponent* animation = target ? target->GetAnimationComponent() : nullptr;
	const K4E::HumanoidVisualComponent* visual = target ? target->GetCharacterComponent<K4E::HumanoidVisualComponent>() : nullptr;
	const K4E::Vector3 targetPosition = target ? target->GetTargetPosition() : K4E::Vector3{};
	const K4E::Vector3 velocity = movement ? movement->GetVelocity() : K4E::Vector3{};
	ImGui::Text("対象Actor: %s", target ? target->GetName().c_str() : "なし");
	ImGui::Text("生存状態: %s", target && target->IsAlive() ? "生存" : "死亡");
	ImGui::Text("HP: %.1f / %.1f", health ? health->GetCurrentHealth() : 0.0f, health ? health->GetMaxHealth() : 0.0f);
	ImGui::Text("ターゲット位置: %.2f, %.2f, %.2f", targetPosition.x, targetPosition.y, targetPosition.z);
	ImGui::Text("移動速度: %.2f, %.2f, %.2f", velocity.x, velocity.y, velocity.z);
	const K4E::Vector3 colliderPosition = collider ? collider->GetWorldPosition() : K4E::Vector3{};
	ImGui::Text("当たり判定位置: %.2f, %.2f, %.2f", colliderPosition.x, colliderPosition.y, colliderPosition.z);
	ImGui::Text("アニメーション: %s / %.2f秒", animation ? animation->GetAnimationName().c_str() : "なし",
		animation ? animation->GetPlaybackTime() : 0.0f);
	ImGui::Text("更新経路: %s", K4E::EditorPlayController::GetInstance()->GetPlayStateText());
	ImGui::Text("人型定義: %s", visual ? visual->GetDefinitionPath().c_str() : "なし");
	ImGui::Text("人型部位数: %zu / Shadow: %s", visual ? visual->GetParts().size() : 0,
		visual && visual->IsCastShadowEnabled() ? "有効" : "無効");
	if (visual)
	{
		const auto* body = visual->FindPart("Body");
		const auto* head = visual->FindPart("Head");
		ImGui::Text("胴体位置: %.2f, %.2f, %.2f", body ? body->transform.worldTranslate_.x : 0.0f,
			body ? body->transform.worldTranslate_.y : 0.0f, body ? body->transform.worldTranslate_.z : 0.0f);
		ImGui::Text("頭位置: %.2f, %.2f, %.2f", head ? head->transform.worldTranslate_.x : 0.0f,
			head ? head->transform.worldTranslate_.y : 0.0f, head ? head->transform.worldTranslate_.z : 0.0f);
	}
	ImGui::Text("死亡イベント回数: %d", deathEventCount_);
	ImGui::Text("直前の適用ダメージ: %.1f", lastAppliedDamage_);
	ImGui::Text("直前の死亡ダメージ: %.1f", lastDeathDamage_);
	ImGui::TextColored(lastSucceeded_ ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
		"%s", lastMessage_.c_str());

	if (ImGui::CollapsingHeader("人型表示の確認結果", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawValidationResult("Component構成", humanoidResults_.composition);
		DrawValidationResult("人型モデル生成", humanoidResults_.models);
		DrawValidationResult("Actor Transform追従", humanoidResults_.actorTransform);
		DrawValidationResult("部位親子追従", humanoidResults_.partHierarchy);
		DrawValidationResult("部位表示切り替え", humanoidResults_.partVisibility);
		DrawValidationResult("Shadow設定", humanoidResults_.shadow);
		DrawValidationResult("Collider追従", humanoidResults_.colliderFollow);
		DrawValidationResult("人型定義JSON", humanoidResults_.definitionJson);
		DrawValidationResult("Actor JSON往復", humanoidResults_.actorJson);
		DrawValidationResult("Editor / Play更新", humanoidResults_.editorAndPlay);
	}

	if (ImGui::Button("自動検証##CharacterValidation")) requestValidation_ = true;
	ImGui::SameLine();
	if (ImGui::Button("キャラクター生成##CharacterValidation")) requestSpawn_ = true;
	ImGui::SameLine();
	if (ImGui::Button("25ダメージ##CharacterValidation")) requestDamage_ = true;

	if (ImGui::Button("致死ダメージ##CharacterValidation")) requestLethalDamage_ = true;
	ImGui::SameLine();
	if (ImGui::Button("全回復##CharacterValidation")) requestRestore_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動開始##CharacterValidation")) requestMove_ = true;
	ImGui::SameLine();
	if (ImGui::Button("移動停止##CharacterValidation")) requestStop_ = true;

	if (ImGui::Button("JSON保存##CharacterValidation")) requestSave_ = true;
	ImGui::SameLine();
	if (ImGui::Button("JSON再読込##CharacterValidation")) requestReload_ = true;

	if (ImGui::Button("人型定義再読込##CharacterValidation")) requestReloadDefinition_ = true;
	ImGui::SameLine();
	if (ImGui::Button("頭 表示切替##CharacterValidation")) requestToggleHead_ = true;
	ImGui::SameLine();
	if (ImGui::Button("左腕 表示切替##CharacterValidation")) requestToggleLeftArm_ = true;
	ImGui::SameLine();
	if (ImGui::Button("右腕 表示切替##CharacterValidation")) requestToggleRightArm_ = true;
	if (ImGui::Button("全部位を表示##CharacterValidation")) requestShowAllParts_ = true;
	ImGui::TextDisabled("人型表示、Transform階層、Shadow、Collider、定義JSON、Actor JSONをまとめて確認できます。");
	ImGui::End();
#endif
}

void CharacterValidation::Finalize()
{
	actorWorld_ = nullptr;
	listenerActor_ = nullptr;
	listenerId_ = 0;
}

K4E::HumanoidVisualComponent* CharacterValidation::FindHumanoidVisual() const
{
	K4E::CharacterActor* target = FindTarget();
	return target ? target->GetCharacterComponent<K4E::HumanoidVisualComponent>() : nullptr;
}

K4E::CharacterActor* CharacterValidation::FindTarget() const
{
	if (!actorWorld_) return nullptr;
	for (const auto& actor : actorWorld_->GetActors())
	{
		if (actor && actor->GetName() == targetActorName_) return dynamic_cast<K4E::CharacterActor*>(actor.get());
	}
	return nullptr;
}

void CharacterValidation::ConfigureTarget(K4E::CharacterActor& actor)
{
	actor.SetName(targetActorName_);
	actor.SetLayer("DebugValidation");
	actor.AddTag("CharacterValidation");
	if (listenerActor_ != &actor || !actor.HasDeathListener(listenerId_))
	{
		listenerId_ = actor.AddDeathListener([this](const K4E::CharacterDeathEvent& deathEvent)
			{
				++deathEventCount_;
				lastDeathDamage_ = deathEvent.result.appliedDamage;
			});
		listenerActor_ = &actor;
	}
	if (K4E::SceneComponent* root = actor.GetRootComponent())
	{
		root->SetLocalPosition({ 4.0f, 0.0f, 0.0f });
		root->RefreshWorldTransform();

		K4E::HumanoidVisualComponent* visual = actor.GetCharacterComponent<K4E::HumanoidVisualComponent>();
		if (!visual)
		{
			visual = &actor.AddComponent<K4E::HumanoidVisualComponent>();
			visual->SetDefinitionPath("Resources/JSON/Characters/DefaultHumanoid.json"); // 検証Actorだけへ標準人型定義を明示的に接続する。
		}
		visual->SetName("Humanoid Visual");
		visual->SetUpdateOrder(-40);
		visual->SetDrawOrder(0);
		visual->SetCastShadowEnabled(true);
		visual->AttachTo(root);
	}
}

void CharacterValidation::TogglePartVisibility(const char* partId)
{
	K4E::HumanoidVisualComponent* visual = FindHumanoidVisual();
	K4E::HumanoidVisualComponent::BodyPart* part = visual ? visual->FindPart(partId ? partId : "") : nullptr;
	lastSucceeded_ = part && visual->SetPartVisible(part->id, !part->visible);
	lastMessage_ = lastSucceeded_
		? part->id + std::string("の表示状態を切り替えました。")
		: std::string("表示を切り替える人型部位が見つかりません。");
}

void CharacterValidation::RunValidation()
{
	K4E::CharacterActor* target = FindTarget();
	if (target)
	{
		ConfigureTarget(*target);
		target->InitializeComponents(); // 実行中に追加した人型表示Componentも検証前に初期化する。
	}
	K4E::CharacterHealthComponent* health = target ? target->GetHealthComponent() : nullptr;
	K4E::CharacterMovementComponent* movement = target ? target->GetMovementComponent() : nullptr;
	K4E::CharacterTargetComponent* targetComponent = target ? target->GetTargetComponent() : nullptr;
	K4E::CharacterColliderComponent* collider = target ? target->GetColliderComponent() : nullptr;
	K4E::CharacterAnimationComponent* animation = target ? target->GetAnimationComponent() : nullptr;
	K4E::SceneComponent* root = target ? target->GetRootComponent() : nullptr;
	K4E::HumanoidVisualComponent* visual = FindHumanoidVisual();
	if (!target || !health || !movement || !targetComponent || !collider || !animation || !root || !visual)
	{
		lastSucceeded_ = false;
		lastMessage_ = "検証に必要なキャラクター構成または人型表示Componentが見つかりません。";
		return;
	}

	humanoidResults_ = {};
	const float originalMaxHealth = health->GetMaxHealth();
	const float originalCurrentHealth = health->GetCurrentHealth();
	const bool originalInvulnerable = health->IsInvulnerable();
	const K4E::Vector3 originalVelocity = movement->GetVelocity();
	const bool originalMovementEnabled = movement->IsMovementEnabled();
	const K4E::Vector3 originalRootPosition = root->GetLocalPosition();
	const K4E::Vector3 originalColliderPosition = collider->GetLocalPosition();
	const K4E::Vector3 originalColliderHalfSize = collider->GetHalfSize();
	const K4E::ECollisionShapeType originalColliderShape = collider->GetShapeType();
	const std::string originalAnimationName = animation->GetAnimationName();
	const float originalAnimationDuration = animation->GetDuration();
	const float originalAnimationTime = animation->GetPlaybackTime();
	const float originalAnimationSpeed = animation->GetPlaybackSpeed();
	const bool originalAnimationLoop = animation->IsLooping();
	const bool originalAnimationPlaying = animation->IsPlaying();
	const K4E::HumanoidDefinition originalDefinition = visual->GetDefinition();
	const std::string originalDefinitionPath = visual->GetDefinitionPath();
	const K4E::Vector3 originalVisualPosition = visual->GetLocalPosition();
	const K4E::Vector3 originalVisualRotation = visual->GetLocalRotation();
	const K4E::Vector3 originalVisualScale = visual->GetLocalScale();
	const bool originalCastShadow = visual->IsCastShadowEnabled();
	const int deathEventsBefore = deathEventCount_;
	int removedListenerCallCount = 0;
	const K4E::CharacterActor::DeathListenerId removedListenerId = target->AddDeathListener(
		[&removedListenerCallCount](const K4E::CharacterDeathEvent&) { ++removedListenerCallCount; });
	const bool listenerRemoved = target->RemoveDeathListener(removedListenerId);

	const std::array<std::string_view, 6> requiredPartIds{
		"Body", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg"
	};
	humanoidResults_.composition = visual->GetParent() == root;
	humanoidResults_.models = std::all_of(requiredPartIds.begin(), requiredPartIds.end(),
		[visual](std::string_view partId)
		{
			const K4E::HumanoidVisualComponent::BodyPart* part = visual->FindPart(partId);
			return part && part->object;
		});

	root->SetLocalPosition({ 4.0f, 0.0f, 0.0f });
	root->RefreshWorldTransform();
	visual->UpdateEditor(0.0f);
	const K4E::HumanoidVisualComponent::BodyPart* body = visual->FindPart("Body");
	const K4E::HumanoidVisualComponent::BodyPart* head = visual->FindPart("Head");
	const K4E::Vector3 bodyBeforeMove = body ? body->transform.worldTranslate_ : K4E::Vector3{};
	const K4E::Vector3 headBeforeMove = head ? head->transform.worldTranslate_ : K4E::Vector3{};
	const K4E::Vector3 actorMoveDelta{ 1.25f, 0.5f, -0.75f };
	root->SetLocalPosition({ 5.25f, 0.5f, -0.75f });
	root->RefreshWorldTransform();
	visual->UpdateEditor(0.0f);
	body = visual->FindPart("Body");
	head = visual->FindPart("Head");
	const K4E::Vector3 bodyMove = body
		? body->transform.worldTranslate_ - bodyBeforeMove
		: K4E::Vector3{};
	const K4E::Vector3 headMove = head
		? head->transform.worldTranslate_ - headBeforeMove
		: K4E::Vector3{};
	humanoidResults_.actorTransform = body && head && NearlyEqual(bodyMove, actorMoveDelta) && NearlyEqual(headMove, actorMoveDelta);

	const K4E::HumanoidVisualComponent::BodyPart* leftArm = visual->FindPart("LeftArm");
	const K4E::HumanoidVisualComponent::BodyPart* rightArm = visual->FindPart("RightArm");
	humanoidResults_.partHierarchy = body && head && leftArm && rightArm &&
		head->parentId == "Body" && leftArm->parentId == "Body" && rightArm->parentId == "Body" &&
		head->transform.parent_ == &body->transform && leftArm->transform.parent_ == &body->transform &&
		rightArm->transform.parent_ == &body->transform;
	const bool headHidden = visual->SetPartVisible("Head", false);
	const K4E::HumanoidVisualComponent::BodyPart* hiddenHead = visual->FindPart("Head");
	const K4E::HumanoidPartDefinition* hiddenHeadDefinition = visual->GetDefinition().FindPart("Head");
	humanoidResults_.partVisibility = headHidden && hiddenHead && !hiddenHead->visible &&
		hiddenHeadDefinition && !hiddenHeadDefinition->visible;
	humanoidResults_.shadow = visual->SupportsShadowCasting() && visual->IsCastShadowEnabled() && humanoidResults_.models;

	health->ResetHealth(100.0f);
	health->SetInvulnerable(false);
	const K4E::CharacterDamageResult nonLethalResult = target->ApplyDamage(25.0f);
	const K4E::CharacterDamageResult lethalResult = target->ApplyDamage(100.0f);
	const K4E::CharacterDamageResult duplicateResult = target->ApplyDamage(1.0f);
	lastAppliedDamage_ = lethalResult.appliedDamage;

	root->SetLocalPosition({ 4.0f, 0.0f, 0.0f });
	root->RefreshWorldTransform();
	movement->SetMovementEnabled(true);
	movement->SetVelocity({ 2.0f, 0.0f, 0.0f });
	movement->Update(0.5f);
	const bool transformMovementSucceeded = std::abs(root->GetLocalPosition().x - 5.0f) < 0.0001f;

	collider->SetShapeType(K4E::ECollisionShapeType::OBB);
	collider->SetHalfSize({ 0.55f, 0.95f, 0.45f });
	collider->SetLocalPosition({ 0.0f, 0.1f, 0.0f });
	collider->UpdateEditor(0.0f);
	const K4E::Vector3 colliderWorldPosition = collider->GetWorldPosition();
	const K4E::Vector3 physicsCenter = collider->GetCollider() ? collider->GetCollider()->GetCenterPosition() : K4E::Vector3{};
	const bool colliderFollowSucceeded = collider->GetCollider() &&
		K4E::Vector3::Length(colliderWorldPosition - physicsCenter) < 0.0001f;
	humanoidResults_.colliderFollow = colliderFollowSucceeded;

	animation->Play("Walk", 2.5f, true);
	animation->SetPlaybackSpeed(1.25f);
	animation->Update(0.4f);
	const bool animationUpdateSucceeded = std::abs(animation->GetPlaybackTime() - 0.5f) < 0.0001f;

	K4E::HumanoidDefinition modifiedDefinition = originalDefinition;
	K4E::HumanoidPartDefinition* modifiedHead = modifiedDefinition.FindPart("Head");
	const float expectedHeadHeight = modifiedHead ? modifiedHead->localPosition.y + 0.125f : 0.0f;
	if (modifiedHead)
	{
		modifiedHead->localPosition.y = expectedHeadHeight;
		modifiedHead->visible = true; // 外部定義の表示値とActor個体の非表示Overrideを分けて復元検証する。
	}
	std::string definitionError;
	const bool definitionSaved = modifiedHead && modifiedDefinition.SaveToFile(validationDefinitionPath_, &definitionError);
	const bool definitionLoaded = definitionSaved && visual->LoadDefinitionFromFile(validationDefinitionPath_, &definitionError);
	const K4E::HumanoidPartDefinition* loadedHeadDefinition = visual->GetDefinition().FindPart("Head");
	const K4E::HumanoidVisualComponent::BodyPart* loadedHead = visual->FindPart("Head");
	humanoidResults_.definitionJson = definitionLoaded && loadedHeadDefinition && loadedHead &&
		std::abs(loadedHeadDefinition->localPosition.y - expectedHeadHeight) <= kValidationEpsilon &&
		std::abs(loadedHead->transform.translate_.y - expectedHeadHeight) <= kValidationEpsilon;

	visual->UpdateEditor(0.0f);
	const K4E::HumanoidVisualComponent::BodyPart* editorBody = visual->FindPart("Body");
	const K4E::HumanoidVisualComponent::BodyPart* editorHead = visual->FindPart("Head");
	const bool editorUpdateSucceeded = editorBody && editorHead &&
		IsFinite(editorBody->transform.worldTranslate_) && IsFinite(editorHead->transform.worldTranslate_);
	visual->Update(1.0f / 60.0f);
	const K4E::HumanoidVisualComponent::BodyPart* playBody = visual->FindPart("Body");
	const K4E::HumanoidVisualComponent::BodyPart* playHead = visual->FindPart("Head");
	humanoidResults_.editorAndPlay = editorUpdateSucceeded && playBody && playHead &&
		IsFinite(playBody->transform.worldTranslate_) && IsFinite(playHead->transform.worldTranslate_);

	// Actor JSONには外部定義への参照と個体固有の部位表示状態を同時に保存する。
	visual->SetPartVisible("Head", false);

	const nlohmann::json snapshot = K4E::ActorJsonSerializer::SerializeActor(*target);
	bool hasHealthJson = false;
	bool hasMovementJson = false;
	bool hasTargetJson = false;
	bool hasColliderJson = false;
	bool hasAnimationJson = false;
	bool hasHumanoidVisualJson = false;
	bool hasHiddenHeadJson = false;
	if (snapshot.contains("Components") && snapshot["Components"].is_array())
	{
		for (const auto& componentJson : snapshot["Components"])
		{
			const std::string className = componentJson.value("Class", std::string{});
			hasHealthJson = hasHealthJson || className == "CharacterHealthComponent";
			hasMovementJson = hasMovementJson || className == "CharacterMovementComponent";
			hasTargetJson = hasTargetJson || className == "CharacterTargetComponent";
			hasColliderJson = hasColliderJson || className == "CharacterColliderComponent";
			hasAnimationJson = hasAnimationJson || className == "CharacterAnimationComponent";
			if (className == "HumanoidVisualComponent")
			{
				hasHumanoidVisualJson = componentJson.value("DefinitionPath", std::string{}) == validationDefinitionPath_;
				const auto visibilityIt = componentJson.find("PartVisibility");
				hasHiddenHeadJson = visibilityIt != componentJson.end() && visibilityIt->is_object() &&
					visibilityIt->value("Head", true) == false;
			}
		}
	}

	std::unique_ptr<K4E::Actor> restoredActor = K4E::ActorJsonSerializer::CreateActorFromJson(snapshot);
	K4E::CharacterActor* restoredCharacter = dynamic_cast<K4E::CharacterActor*>(restoredActor.get());
	const K4E::CharacterColliderComponent* restoredCollider = restoredCharacter ? restoredCharacter->GetColliderComponent() : nullptr;
	const K4E::CharacterAnimationComponent* restoredAnimation = restoredCharacter ? restoredCharacter->GetAnimationComponent() : nullptr;
	const K4E::HumanoidVisualComponent* restoredVisual = restoredCharacter
		? restoredCharacter->GetCharacterComponent<K4E::HumanoidVisualComponent>()
		: nullptr;
	const bool colliderJsonRestored = restoredCollider && restoredCollider->GetShapeType() == K4E::ECollisionShapeType::OBB &&
		std::abs(restoredCollider->GetHalfSize().x - 0.55f) < 0.0001f &&
		std::abs(restoredCollider->GetHalfSize().y - 0.95f) < 0.0001f;
	const bool animationJsonRestored = restoredAnimation && restoredAnimation->GetAnimationName() == "Walk" &&
		std::abs(restoredAnimation->GetDuration() - 2.5f) < 0.0001f &&
		std::abs(restoredAnimation->GetPlaybackTime() - 0.5f) < 0.0001f &&
		std::abs(restoredAnimation->GetPlaybackSpeed() - 1.25f) < 0.0001f;
	const K4E::HumanoidVisualComponent::BodyPart* restoredHead = restoredVisual ? restoredVisual->FindPart("Head") : nullptr;
	const K4E::HumanoidPartDefinition* restoredHeadDefinition = restoredVisual
		? restoredVisual->GetDefinition().FindPart("Head")
		: nullptr;
	const bool humanoidJsonRestored = restoredVisual && restoredCharacter &&
		restoredVisual->GetParent() == restoredCharacter->GetRootComponent() && restoredHead && !restoredHead->visible &&
		restoredHeadDefinition && std::abs(restoredHeadDefinition->localPosition.y - expectedHeadHeight) <= kValidationEpsilon &&
		restoredVisual->GetDefinitionPath() == validationDefinitionPath_;
	const bool factoryRestoreSucceeded = restoredCharacter && restoredCharacter->GetHealthComponent() &&
		restoredCharacter->GetMovementComponent() && restoredCharacter->GetTargetComponent() &&
		colliderJsonRestored && animationJsonRestored && humanoidJsonRestored;
	humanoidResults_.actorJson = snapshot.value("Class", std::string{}) == "CharacterActor" &&
		hasHealthJson && hasMovementJson && hasTargetJson && hasColliderJson && hasAnimationJson &&
		hasHumanoidVisualJson && hasHiddenHeadJson && factoryRestoreSucceeded;
	if (restoredActor) restoredActor->FinalizeForWorld();

	const K4E::Vector3 actorTargetPosition = target->GetTargetPosition();
	const K4E::Vector3 componentTargetPosition = targetComponent->GetTargetPosition();
	const bool targetPositionMatches = actorTargetPosition.x == componentTargetPosition.x &&
		actorTargetPosition.y == componentTargetPosition.y && actorTargetPosition.z == componentTargetPosition.z;
	const K4E::Vector3 explicitTargetPosition{ 8.0f, 2.0f, -3.0f };
	targetComponent->SetTargetPosition(explicitTargetPosition);
	const K4E::Vector3 delegatedTargetPosition = target->GetTargetPosition();
	const bool explicitTargetMatches = targetComponent->HasExplicitTargetPosition() &&
		delegatedTargetPosition.x == explicitTargetPosition.x && delegatedTargetPosition.y == explicitTargetPosition.y &&
		delegatedTargetPosition.z == explicitTargetPosition.z;
	targetComponent->ClearTargetPosition();
	const bool componentAccessMatches = target->GetCharacterComponent<K4E::CharacterHealthComponent>() == health;
	const bool movementDelegationMatches = target->GetMovementComponent() == movement &&
		movement->IsMovementEnabled() && movement->GetVelocity().x == 2.0f && transformMovementSucceeded;

	const bool succeeded = listenerRemoved && removedListenerCallCount == 0 && componentAccessMatches && movementDelegationMatches &&
		colliderFollowSucceeded && animationUpdateSucceeded && targetPositionMatches && explicitTargetMatches &&
		nonLethalResult.accepted && nonLethalResult.healthBefore == 100.0f &&
		nonLethalResult.healthAfter == 75.0f && !nonLethalResult.killed && lethalResult.accepted && lethalResult.killed &&
		lethalResult.healthAfter == 0.0f && !duplicateResult.accepted && deathEventCount_ == deathEventsBefore + 1 &&
		snapshot.value("Class", std::string{}) == "CharacterActor" && hasHealthJson && hasMovementJson && hasTargetJson &&
		hasColliderJson && hasAnimationJson && factoryRestoreSucceeded && humanoidResults_.AllSucceeded();

	health->SetMaxHealth(originalMaxHealth);
	health->SetCurrentHealth(originalCurrentHealth);
	health->SetInvulnerable(originalInvulnerable);
	movement->SetVelocity(originalVelocity);
	movement->SetMovementEnabled(originalMovementEnabled);
	root->SetLocalPosition(originalRootPosition);
	root->RefreshWorldTransform();
	collider->SetShapeType(originalColliderShape);
	collider->SetHalfSize(originalColliderHalfSize);
	collider->SetLocalPosition(originalColliderPosition);
	collider->UpdateEditor(0.0f);
	animation->Play(originalAnimationName, originalAnimationDuration, originalAnimationLoop);
	animation->SetPlaybackSpeed(originalAnimationSpeed);
	animation->SetPlaybackTime(originalAnimationTime);
	if (!originalAnimationPlaying) animation->Pause();
	visual->SetDefinitionPath(originalDefinitionPath);
	visual->SetDefinition(originalDefinition);
	visual->SetLocalPosition(originalVisualPosition);
	visual->SetLocalRotation(originalVisualRotation);
	visual->SetLocalScale(originalVisualScale);
	visual->SetCastShadowEnabled(originalCastShadow);
	visual->UpdateEditor(0.0f); // 自動検証後は画面上の人型を検証前の状態へ戻す。

	std::ostringstream message;
	message << (succeeded ? "キャラクター共通機能の自動検証に成功しました。" : "キャラクター共通機能の自動検証で不整合を検出しました。")
		<< " HP=" << health->GetCurrentHealth() << "/" << health->GetMaxHealth()
		<< " DeathEvent=" << deathEventCount_
		<< " Collider=" << (colliderFollowSucceeded ? "OK" : "NG")
		<< " Visual=" << (humanoidResults_.AllSucceeded() ? "OK" : "NG")
		<< " DefinitionJSON=" << (humanoidResults_.definitionJson ? "OK" : "NG")
		<< " ActorJSON=" << (humanoidResults_.actorJson ? "OK" : "NG")
		<< " EditorPlay=" << (humanoidResults_.editorAndPlay ? "OK" : "NG");
	lastSucceeded_ = succeeded;
	lastMessage_ = message.str();
}
