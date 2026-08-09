#pragma once

#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"

#include <ActorComponent.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// DebugScene専用として、新Playerの主要Runtime機能をフレーム跨ぎで一括検証するComponent。
	class PlayerMigrationValidationComponent final : public ActorComponent
	{
	public:
		void Initialize() override
		{
			ResetValidationState();
		}

		void Update(float deltaTime) override
		{
			PlayerActor* player = GetPlayer();
			if (!player)
			{
				Fail("PlayerMigrationValidationComponentのOwnerがPlayerActorではありません。");
				return;
			}

			if (requestPlayerReset_)
			{
				requestPlayerReset_ = false;
				ResetPlayer(*player);
				ResetValidationState();
				lastMessage_ = "Playerを検証開始前の通常状態へ戻しました。";
			}

			if (requestRun_)
			{
				requestRun_ = false;
				BeginValidation(*player);
			}

			if (!running_) return;
			stepElapsed_ += (std::max)(0.0f, deltaTime);

			switch (stage_)
			{
			case Stage::Structure:
				ValidateStructure(*player);
				break;
			case Stage::Damage:
				ValidateDamage(*player);
				break;
			case Stage::Heal:
				ValidateHeal(*player);
				break;
			case Stage::FireRequest:
				BeginFireValidation(*player);
				break;
			case Stage::FireVerify:
				VerifyFire(*player);
				break;
			case Stage::ReloadRequest:
				BeginReloadValidation(*player);
				break;
			case Stage::ReloadWait:
				VerifyReload(*player);
				break;
			case Stage::Death:
				ValidateDeath(*player);
				break;
			case Stage::GameOverWait:
				ValidateGameOverReady(*player);
				break;
			case Stage::Reset:
				ValidateReset(*player);
				break;
			case Stage::Idle:
			case Stage::Complete:
			case Stage::Failed:
			default:
				break;
			}
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("P9 Player全機能検証");
			PlayerActor* player = GetPlayer();
			if (!player)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.3f, 1.0f), "PlayerActor Ownerが見つかりません。");
				return;
			}

			ImGui::Text("Stage: %s", GetStageName());
			ImGui::Text("Progress: %d / %d", passedChecks_, kTotalChecks);
			ImGui::Text("HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
			if (const PlayerMovementComponent* movement = player->GetPlayerMovementComponent())
			{
				ImGui::Text("Grounded: %s / Blink: %s", movement->IsGrounded() ? "Yes" : "No", movement->IsBlinking() ? "Yes" : "No");
			}
			if (const WeaponComponent* weapon = player->GetWeaponComponent())
			{
				ImGui::Text("Ammo: %d / %d  Reserve: %d  Mode: %s", weapon->GetMagazineAmmo(), weapon->GetMagazineCapacity(), weapon->GetReserveAmmo(), weapon->IsAutomaticFireMode() ? "AUTO" : "SEMI");
			}

			const ImVec4 statusColor = lastSucceeded_
				? ImVec4(0.35f, 1.0f, 0.45f, 1.0f)
				: (stage_ == Stage::Failed ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			ImGui::TextColored(statusColor, "%s", lastMessage_.c_str());

			if (running_) ImGui::BeginDisabled();
			if (ImGui::Button("P9 全自動検証")) RequestRunFullValidation();
			if (running_) ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Player Reset")) RequestPlayerReset();
			ImGui::TextDisabled("F10でも全自動検証を開始できます。Damage/Heal/Fire/Reload/Death/GameOver/Resetを順番に確認します。");
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerMigrationValidationComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson); // 検証中の一時状態はPIE Snapshotへ持ち越さず、Component構成だけ保存する。
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			ResetValidationState();
		}

		void RequestRunFullValidation() { requestRun_ = true; }
		void RequestPlayerReset() { requestPlayerReset_ = true; }
		bool IsRunning() const { return running_; }
		bool IsInputLocked() const { return running_ || requestRun_; }
		bool WasLastRunSuccessful() const { return lastSucceeded_; }
		const std::string& GetLastMessage() const { return lastMessage_; }

	private:
		enum class Stage
		{
			Idle,
			Structure,
			Damage,
			Heal,
			FireRequest,
			FireVerify,
			ReloadRequest,
			ReloadWait,
			Death,
			GameOverWait,
			Reset,
			Complete,
			Failed,
		};

		PlayerActor* GetPlayer() const
		{
			return dynamic_cast<PlayerActor*>(GetOwner());
		}

		void BeginValidation(PlayerActor& player)
		{
			if (const SceneComponent* root = player.GetRootComponent()) resetPosition_ = root->GetLocalPosition();
			ResetPlayer(player);
			if (PlayerInputComponent* input = player.GetPlayerInputComponent()) input->SetInputEnabled(false); // 自動検証中は実入力を遮断して結果を再現可能にする。
			running_ = true;
			lastSucceeded_ = false;
			passedChecks_ = 0;
			stage_ = Stage::Structure;
			stepElapsed_ = 0.0f;
			lastMessage_ = "P9自動検証を開始しました。";
		}

		void ValidateStructure(PlayerActor& player)
		{
			const SceneComponent* root = player.GetRootComponent();
			const PlayerInputComponent* input = player.GetPlayerInputComponent();
			const PlayerMovementComponent* movement = player.GetPlayerMovementComponent();
			const PlayerCameraComponent* camera = player.GetPlayerCameraComponent();
			const WeaponComponent* weapon = player.GetWeaponComponent();
			const InventoryComponent* inventory = player.GetInventoryComponent();
			const PlayerHudPresenterComponent* hud = player.GetPlayerHudPresenterComponent();
			const CharacterHealthComponent* health = player.GetHealthComponent();
			const CharacterColliderComponent* collider = player.GetColliderComponent();
			const RigidbodyComponent* rigidbody = player.GetRigidbodyComponent();

			const bool succeeded = root && input && movement && camera && weapon && inventory && hud && health && collider && rigidbody &&
				player.GetHumanoidVisualComponent() && player.GetWeaponViewComponent() && camera->GetCamera() && camera->GetParent() == root &&
				collider->GetCollider() && rigidbody->GetRigidbody() && health->GetMaxHealth() > 0.0f;
			if (!succeeded)
			{
				Fail("構成検証失敗: Player必須Component、Camera親子関係、Physics実体のいずれかが不足しています。");
				return;
			}
			Pass("1/8 構成・Camera・Physics接続を確認しました。", Stage::Damage);
		}

		void ValidateDamage(PlayerActor& player)
		{
			const float before = player.GetHP();
			const CharacterDamageResult result = player.ApplyPlayerDamage(25.0f);
			if (!result.accepted || result.appliedDamage <= 0.0f || !(player.GetHP() < before))
			{
				Fail("Damage検証失敗: HPが減少しませんでした。");
				return;
			}
			Pass("2/8 DamageとHP減少を確認しました。", Stage::Heal);
		}

		void ValidateHeal(PlayerActor& player)
		{
			const float healed = player.HealPlayer(25.0f);
			if (healed <= 0.0f || std::abs(player.GetHP() - player.GetMaxHP()) > 0.01f)
			{
				Fail("Heal検証失敗: HPが最大値まで復帰しませんでした。");
				return;
			}
			Pass("3/8 HealとHP復帰を確認しました。", Stage::FireRequest);
		}

		void BeginFireValidation(PlayerActor& player)
		{
			WeaponComponent* weapon = player.GetWeaponComponent();
			if (!weapon)
			{
				Fail("Fire検証失敗: WeaponComponentがありません。");
				return;
			}
			weapon->ResetWeapon();
			shotRevisionBefore_ = weapon->GetShotRevision();
			ammoBefore_ = weapon->GetMagazineAmmo();
			weapon->RequestFire();
			SetStage(Stage::FireVerify, "4/8 射撃要求を送信しました。次フレームで発射成立を確認します。");
		}

		void VerifyFire(PlayerActor& player)
		{
			WeaponComponent* weapon = player.GetWeaponComponent();
			if (!weapon || weapon->GetShotRevision() != shotRevisionBefore_ + 1 || weapon->GetMagazineAmmo() != ammoBefore_ - 1)
			{
				Fail("Fire検証失敗: ShotRevisionまたは弾数が更新されませんでした。");
				return;
			}
			Pass("4/8 射撃成立と弾薬消費を確認しました。", Stage::ReloadRequest);
		}

		void BeginReloadValidation(PlayerActor& player)
		{
			WeaponComponent* weapon = player.GetWeaponComponent();
			if (!weapon)
			{
				Fail("Reload検証失敗: WeaponComponentがありません。");
				return;
			}
			reserveBeforeReload_ = weapon->GetReserveAmmo();
			weapon->RequestReload();
			SetStage(Stage::ReloadWait, "5/8 Reload要求を送信しました。完了を待機しています。");
		}

		void VerifyReload(PlayerActor& player)
		{
			WeaponComponent* weapon = player.GetWeaponComponent();
			if (!weapon)
			{
				Fail("Reload検証失敗: WeaponComponentが失われました。");
				return;
			}
			if (stepElapsed_ > kReloadTimeoutSeconds)
			{
				Fail("Reload検証失敗: タイムアウトしました。");
				return;
			}
			if (weapon->IsReloading()) return;
			if (weapon->GetMagazineAmmo() != weapon->GetMagazineCapacity() || weapon->GetReserveAmmo() >= reserveBeforeReload_)
			{
				Fail("Reload検証失敗: MagazineまたはReserveが期待値へ更新されませんでした。");
				return;
			}
			Pass("5/8 Reload完了とReserve消費を確認しました。", Stage::Death);
		}

		void ValidateDeath(PlayerActor& player)
		{
			PlayerInputComponent* input = player.GetPlayerInputComponent();
			PlayerMovementComponent* movement = player.GetPlayerMovementComponent();
			WeaponComponent* weapon = player.GetWeaponComponent();
			CharacterColliderComponent* collider = player.GetColliderComponent();
			if (!input || !movement || !weapon || !collider)
			{
				Fail("Death検証失敗: 停止対象Componentがありません。");
				return;
			}

			input->SetInputEnabled(true);
			movement->SetMovementEnabled(true);
			weapon->SetWeaponEnabled(true);
			collider->SetActive(true);
			player.ApplyPlayerDamage(player.GetMaxHP() * 2.0f);

			const bool stopped = player.IsDeathActive() && !input->IsInputEnabled() && !movement->IsMovementEnabled() && !weapon->IsWeaponEnabled() && !collider->IsActive();
			if (!stopped)
			{
				Fail("Death検証失敗: 死亡後にInput/Movement/Weapon/Colliderの停止が揃いませんでした。");
				return;
			}
			Pass("6/8 致死DamageとPlayer機能停止を確認しました。", Stage::GameOverWait);
		}

		void ValidateGameOverReady(PlayerActor& player)
		{
			if (stepElapsed_ > kGameOverTimeoutSeconds)
			{
				Fail("GameOverReady検証失敗: タイムアウトしました。");
				return;
			}
			if (!player.IsGameOverReady()) return;
			Pass("7/8 GameOverReady遷移を確認しました。", Stage::Reset);
		}

		void ValidateReset(PlayerActor& player)
		{
			ResetPlayer(player);
			const PlayerInputComponent* input = player.GetPlayerInputComponent();
			const PlayerMovementComponent* movement = player.GetPlayerMovementComponent();
			const WeaponComponent* weapon = player.GetWeaponComponent();
			const CharacterColliderComponent* collider = player.GetColliderComponent();
			const bool restored = !player.IsDeathActive() && !player.IsGameOverReady() && std::abs(player.GetHP() - player.GetMaxHP()) <= 0.01f &&
				input && input->IsInputEnabled() && movement && movement->IsMovementEnabled() && weapon && weapon->IsWeaponEnabled() && collider && collider->IsActive();
			if (!restored)
			{
				Fail("Reset検証失敗: Playerの生存・入力・移動・武器・Colliderが完全復帰しませんでした。");
				return;
			}

			++passedChecks_;
			running_ = false;
			lastSucceeded_ = true;
			stage_ = Stage::Complete;
			lastMessage_ = "8/8 P9 Player自動検証に成功しました。GamePlay投入へ進める状態です。";
		}

		void ResetPlayer(PlayerActor& player)
		{
			player.ResetForValidation(resetPosition_); // 自動検証の副作用を残さず、同じ位置から手動確認を再開できるようにする。
		}

		void Pass(const char* message, Stage nextStage)
		{
			++passedChecks_;
			SetStage(nextStage, message);
		}

		void SetStage(Stage stage, const char* message)
		{
			stage_ = stage;
			stepElapsed_ = 0.0f;
			lastMessage_ = message;
		}

		void Fail(const char* message)
		{
			running_ = false;
			lastSucceeded_ = false;
			stage_ = Stage::Failed;
			lastMessage_ = message;
		}

		void ResetValidationState()
		{
			running_ = false;
			requestRun_ = false;
			requestPlayerReset_ = false;
			lastSucceeded_ = false;
			stage_ = Stage::Idle;
			stepElapsed_ = 0.0f;
			passedChecks_ = 0;
			shotRevisionBefore_ = 0;
			ammoBefore_ = 0;
			reserveBeforeReload_ = 0;
			lastMessage_ = "未検証。P9 全自動検証またはF10で開始できます。";
		}

		const char* GetStageName() const
		{
			switch (stage_)
			{
			case Stage::Idle: return "Idle";
			case Stage::Structure: return "Structure";
			case Stage::Damage: return "Damage";
			case Stage::Heal: return "Heal";
			case Stage::FireRequest: return "Fire Request";
			case Stage::FireVerify: return "Fire Verify";
			case Stage::ReloadRequest: return "Reload Request";
			case Stage::ReloadWait: return "Reload Wait";
			case Stage::Death: return "Death";
			case Stage::GameOverWait: return "GameOver Wait";
			case Stage::Reset: return "Reset";
			case Stage::Complete: return "Complete";
			case Stage::Failed: return "Failed";
			default: return "Unknown";
			}
		}

	private:
		static constexpr int kTotalChecks = 8;
		static constexpr float kReloadTimeoutSeconds = 4.0f;
		static constexpr float kGameOverTimeoutSeconds = 3.0f;

		Stage stage_ = Stage::Idle;
		Vector3 resetPosition_{};
		float stepElapsed_ = 0.0f;
		unsigned int shotRevisionBefore_ = 0;
		int ammoBefore_ = 0;
		int reserveBeforeReload_ = 0;
		int passedChecks_ = 0;
		bool running_ = false;
		bool requestRun_ = false;
		bool requestPlayerReset_ = false;
		bool lastSucceeded_ = false;
		std::string lastMessage_ = "未検証。P9 全自動検証またはF10で開始できます。";
	};
} // namespace Ken4lowEngine
