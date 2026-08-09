#define NOMINMAX
#include "BossIntroController.h"

#include "BossAttackEffects.h"
#include "GpuParticleManager.h"
#include "GpuParticleType.h"
#include "ParameterManager.h"
#include <LogString.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void BossIntroController::Initialize(const K4E::Vector3& defaultBossPosition)
{
	settings_.bossAppearPosition = defaultBossPosition;
	bossEnemyVfx_.Initialize(K4E::GpuParticleManager::GetInstance(), "BossIntro");
	RegisterParameters();
	ApplyParameters();
	Reset();
}

void BossIntroController::Finalize()
{
	UnregisterParameters();
	bossEnemyVfx_.Reset();
	hasRequestedBossPosition_ = false;
	ResetBossIntroDustState(); // Scene再生成時にEmitterと演出タイマーを持ち越さない。
}

void BossIntroController::RequestStart(const K4E::Vector3& bossPosition)
{
	if (hasPlayedBossIntro_ || IsRunning()) return;
	ApplyParameters();
	requestedBossPosition_ = bossPosition;
	hasRequestedBossPosition_ = true;
	settings_.bossAppearPosition = requestedBossPosition_; // Stage固有座標をParameter読込後に確定し、演出中の再適用でも維持する。
	stateTimer_ = 0.0f;
	ResetBossIntroDustState();
	ChangeState(State::WaitingAfterCrystalsBroken);
	K4E::Log("[BossIntro] Waiting after objective completed.\n");
}

void BossIntroController::Reset()
{
	state_ = State::None;
	stateTimer_ = 0.0f;
	hasPlayedBossIntro_ = false;
	bossSpawnRequested_ = false;
	bossColliderEnableRequested_ = false;
	debugStartRequested_ = false;
	debugResetRequested_ = false;
	debugForceBossToAppearRequested_ = false;
	debugClearBossParentRequested_ = false;
	debugUseGameplayViewProjectionRequested_ = false;
	hasRequestedBossPosition_ = false;
	requestedBossPosition_ = {};
	ResetBossIntroDustState();
}

void BossIntroController::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("BossIntro", ImGuiTreeNodeFlags_DefaultOpen)) return;
	ImGui::Text("State: %s", ToStateLabel(state_));
	ImGui::Text("Played: %s / Active: %s / Pause: %s", hasPlayedBossIntro_ ? "true" : "false", IsRunning() ? "true" : "false", IsGameplayPaused() ? "true" : "false");
	ImGui::Text("Timer: %.2f", stateTimer_);
	ImGui::Text("Boss Position: %.2f, %.2f, %.2f%s", debugBossPosition_.x, debugBossPosition_.y, debugBossPosition_.z, debugHasBoss_ ? "" : " (none)");
	ImGui::Text("Boss Local: %.2f, %.2f, %.2f", debugBossLocalPosition_.x, debugBossLocalPosition_.y, debugBossLocalPosition_.z);
	ImGui::Text("Boss World: %.2f, %.2f, %.2f", debugBossWorldPosition_.x, debugBossWorldPosition_.y, debugBossWorldPosition_.z);
	ImGui::Text("Boss Parent nullptr: %s", debugBossHasParent_ ? "false" : "true");
	ImGui::Text("Boss Final: %.2f, %.2f, %.2f", settings_.bossAppearPosition.x, settings_.bossAppearPosition.y, settings_.bossAppearPosition.z);
	ImGui::Text("Camera: %.2f, %.2f, %.2f%s", debugCameraPosition_.x, debugCameraPosition_.y, debugCameraPosition_.z, debugHasCamera_ ? "" : " (none)");
	ImGui::Text("ViewProjection: %s / Distance: %.2f", debugViewProjectionKind_, debugBossCameraDistance_);
	ImGui::Text("Dust: %s / Timer %.2f / Impact %s", settings_.enableBossIntroDust ? "on" : "off", bossIntroDustEmitTimer_, bossSpawnImpactTriggered_ ? "yes" : "no");
	ImGui::Text("Shake: %.2f / %.2f amp %.2f freq %.2f", cameraShakeTimer_, cameraShakeDuration_, cameraShakeAmplitude_, cameraShakeFrequency_);
	if (ImGui::Button("Start Boss Intro")) debugStartRequested_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Reset Boss Intro")) debugResetRequested_ = true;
	if (ImGui::Button("Force Boss To Appear Position")) debugForceBossToAppearRequested_ = true;
	ImGui::SameLine();
	if (ImGui::Button("Clear Boss Parent")) debugClearBossParentRequested_ = true;
	if (ImGui::Button("Use Gameplay ViewProjection")) debugUseGameplayViewProjectionRequested_ = true;
	ImGui::Text("Parameter group: %s", kParameterGroupName);
#endif
}

bool BossIntroController::IsGameplayPaused() const
{
	if (!settings_.bossIntroPauseGame) return false;
	return state_ == State::StartCutscene || state_ == State::CameraMoveToBoss || state_ == State::BossSpawnImpact || state_ == State::BossRising || state_ == State::FinishCutscene;
}

bool BossIntroController::ConsumeBossSpawnRequest()
{
	const bool requested = bossSpawnRequested_;
	bossSpawnRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeBossColliderEnableRequest()
{
	const bool requested = bossColliderEnableRequested_;
	bossColliderEnableRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugStartRequest()
{
	const bool requested = debugStartRequested_;
	debugStartRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugResetRequest()
{
	const bool requested = debugResetRequested_;
	debugResetRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugForceBossToAppearRequest()
{
	const bool requested = debugForceBossToAppearRequested_;
	debugForceBossToAppearRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugClearBossParentRequest()
{
	const bool requested = debugClearBossParentRequested_;
	debugClearBossParentRequested_ = false;
	return requested;
}

bool BossIntroController::ConsumeDebugUseGameplayViewProjectionRequest()
{
	const bool requested = debugUseGameplayViewProjectionRequested_;
	debugUseGameplayViewProjectionRequested_ = false;
	return requested;
}

K4E::Vector3 BossIntroController::GetBossStartPosition() const
{
	K4E::Vector3 start = settings_.bossAppearPosition;
	start.y += settings_.enableBossRiseEffect ? settings_.bossStartOffsetY : 0.0f;
	return start;
}

K4E::Vector3 BossIntroController::GetBossLookTarget() const
{
	K4E::Vector3 target = settings_.bossAppearPosition;
	target.y += 2.5f;
	return target;
}

void BossIntroController::RegisterParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	parameters->CreateGroup(kParameterGroupName);
	parameters->AddItem(kParameterGroupName, "bossAppearDelay", settings_.bossAppearDelay, 0.0f, 20.0f);
	parameters->AddItem(kParameterGroupName, "bossAppearPosition", settings_.bossAppearPosition, K4E::Vector3{ -200.0f, -50.0f, -200.0f }, K4E::Vector3{ 200.0f, 100.0f, 200.0f });
	parameters->AddItem(kParameterGroupName, "bossStartOffsetY", settings_.bossStartOffsetY, -100.0f, 0.0f);
	parameters->AddItem(kParameterGroupName, "bossRiseTime", settings_.bossRiseTime, 0.1f, 20.0f);
	parameters->AddItem(kParameterGroupName, "cameraMoveTime", settings_.cameraMoveTime, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "cameraReturnTime", settings_.cameraReturnTime, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "bossIntroPauseGame", settings_.bossIntroPauseGame);
	parameters->AddItem(kParameterGroupName, "enableBossIntroCamera", settings_.enableBossIntroCamera);
	parameters->AddItem(kParameterGroupName, "enableBossRiseEffect", settings_.enableBossRiseEffect);
	parameters->AddItem(kParameterGroupName, "enableBossIntroDust", settings_.enableBossIntroDust);
	parameters->AddItem(kParameterGroupName, "enableBossIntroCameraShake", settings_.enableBossIntroCameraShake);
	parameters->AddItem(kParameterGroupName, "dustGroundOffsetY", settings_.dustGroundOffsetY, 0.0f, 10.0f);
	parameters->AddItem(kParameterGroupName, "dustEmitInterval", settings_.dustEmitInterval, 0.01f, 1.0f);
	parameters->AddItem(kParameterGroupName, "dustStartBurstCount", static_cast<int>(settings_.dustStartBurstCount), 0, 512);
	parameters->AddItem(kParameterGroupName, "dustLoopEmitCount", static_cast<int>(settings_.dustLoopEmitCount), 0, 128);
	parameters->AddItem(kParameterGroupName, "dustEndBurstCount", static_cast<int>(settings_.dustEndBurstCount), 0, 512);
	parameters->AddItem(kParameterGroupName, "spawnShakeDuration", settings_.spawnShakeDuration, 0.0f, 3.0f);
	parameters->AddItem(kParameterGroupName, "spawnShakeAmplitude", settings_.spawnShakeAmplitude, 0.0f, 2.0f);
	parameters->AddItem(kParameterGroupName, "spawnShakeFrequency", settings_.spawnShakeFrequency, 1.0f, 60.0f);

	parameters->SetDisplayName(kParameterGroupName, "bossAppearDelay", "ボス登場までの遅延");
	parameters->SetDisplayName(kParameterGroupName, "bossAppearPosition", "ボス最終出現座標");
	parameters->SetDisplayName(kParameterGroupName, "bossStartOffsetY", "ボス開始Yオフセット");
	parameters->SetDisplayName(kParameterGroupName, "bossRiseTime", "ボス上昇時間");
	parameters->SetDisplayName(kParameterGroupName, "cameraMoveTime", "カメラ移動時間");
	parameters->SetDisplayName(kParameterGroupName, "cameraReturnTime", "カメラ復帰時間");
	parameters->SetDisplayName(kParameterGroupName, "bossIntroPauseGame", "登場中ゲーム進行停止");
	parameters->SetDisplayName(kParameterGroupName, "enableBossIntroCamera", "登場カメラ有効");
	parameters->SetDisplayName(kParameterGroupName, "enableBossRiseEffect", "下から登場有効");
	parameters->SetDisplayName(kParameterGroupName, "enableBossIntroDust", "登場土煙有効");
	parameters->SetDisplayName(kParameterGroupName, "enableBossIntroCameraShake", "登場カメラ揺れ有効");
	parameters->SetDisplayName(kParameterGroupName, "dustGroundOffsetY", "土煙の足元Y補正");
	parameters->SetDisplayName(kParameterGroupName, "dustEmitInterval", "土煙の連続発生間隔");
	parameters->SetDisplayName(kParameterGroupName, "dustStartBurstCount", "登場開始土煙数");
	parameters->SetDisplayName(kParameterGroupName, "dustLoopEmitCount", "登場中土煙数");
	parameters->SetDisplayName(kParameterGroupName, "dustEndBurstCount", "登場完了土煙数");
	parameters->SetDisplayName(kParameterGroupName, "spawnShakeDuration", "登場カメラ揺れ時間");
	parameters->SetDisplayName(kParameterGroupName, "spawnShakeAmplitude", "登場カメラ揺れ強度");
	parameters->SetDisplayName(kParameterGroupName, "spawnShakeFrequency", "登場カメラ揺れ周波数");
	parameters->LoadFile(kParameterGroupName);
	parameters->RegisterParameterApplier(kParameterGroupName, this, [this]() { ApplyParameters(); });
}

void BossIntroController::UnregisterParameters()
{
	K4E::ParameterManager::GetInstance()->UnregisterParameterApplier(kParameterGroupName, this);
}

void BossIntroController::ApplyParameters()
{
	auto* parameters = K4E::ParameterManager::GetInstance();
	settings_.bossAppearDelay = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "bossAppearDelay"));
	settings_.bossAppearPosition = parameters->GetValue<K4E::Vector3>(kParameterGroupName, "bossAppearPosition");
	settings_.bossStartOffsetY = std::min(0.0f, parameters->GetValue<float>(kParameterGroupName, "bossStartOffsetY"));
	settings_.bossRiseTime = std::max(0.01f, parameters->GetValue<float>(kParameterGroupName, "bossRiseTime"));
	settings_.cameraMoveTime = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "cameraMoveTime"));
	settings_.cameraReturnTime = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "cameraReturnTime"));
	settings_.bossIntroPauseGame = parameters->GetValue<bool>(kParameterGroupName, "bossIntroPauseGame");
	settings_.enableBossIntroCamera = parameters->GetValue<bool>(kParameterGroupName, "enableBossIntroCamera");
	settings_.enableBossRiseEffect = parameters->GetValue<bool>(kParameterGroupName, "enableBossRiseEffect");
	settings_.enableBossIntroDust = parameters->GetValue<bool>(kParameterGroupName, "enableBossIntroDust");
	settings_.enableBossIntroCameraShake = parameters->GetValue<bool>(kParameterGroupName, "enableBossIntroCameraShake");
	settings_.dustGroundOffsetY = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "dustGroundOffsetY"));
	settings_.dustEmitInterval = std::max(0.01f, parameters->GetValue<float>(kParameterGroupName, "dustEmitInterval"));
	settings_.dustStartBurstCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustStartBurstCount")));
	settings_.dustLoopEmitCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustLoopEmitCount")));
	settings_.dustEndBurstCount = static_cast<uint32_t>(std::max(0, parameters->GetValue<int>(kParameterGroupName, "dustEndBurstCount")));
	settings_.spawnShakeDuration = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "spawnShakeDuration"));
	settings_.spawnShakeAmplitude = std::max(0.0f, parameters->GetValue<float>(kParameterGroupName, "spawnShakeAmplitude"));
	settings_.spawnShakeFrequency = std::max(1.0f, parameters->GetValue<float>(kParameterGroupName, "spawnShakeFrequency"));
}

void BossIntroController::ChangeState(State state)
{
	state_ = state;
	stateTimer_ = 0.0f;
}

void BossIntroController::BeginCutscene(K4E::Camera* camera)
{
	if (camera)
	{
		savedCameraPosition_ = camera->GetTranslate();
		savedCameraRotation_ = camera->GetRotate();
	}
	introCameraTarget_ = GetBossLookTarget();
	introCameraPosition_ = settings_.bossAppearPosition + K4E::Vector3{ 0.0f, 7.0f, -18.0f };
	ResetBossIntroDustState();
	ChangeState(settings_.enableBossIntroCamera ? State::CameraMoveToBoss : State::BossSpawnImpact);
	K4E::Log("[BossIntro] Cutscene started.\n");
}

void BossIntroController::UpdateCameraMove(float deltaTime, K4E::Camera* camera)
{
	stateTimer_ += deltaTime;
	const float t = settings_.cameraMoveTime <= 0.0f ? 1.0f : Clamp01(stateTimer_ / settings_.cameraMoveTime);
	if (camera) ApplyCameraLookAtBoss(camera, Lerp(savedCameraPosition_, introCameraPosition_, t), introCameraTarget_);
	if (t >= 1.0f) ChangeState(State::BossSpawnImpact);
}

void BossIntroController::BeginBossSpawnImpact(K4E::Camera* camera)
{
	if (!bossSpawnImpactTriggered_)
	{
		bossSpawnImpactTriggered_ = true;
		bossSpawnRequested_ = true;
		StartCameraShake(settings_.spawnShakeDuration, settings_.spawnShakeAmplitude, settings_.spawnShakeFrequency);
		if (settings_.enableBossIntroCamera && camera) ApplyCameraLookAtBoss(camera, introCameraPosition_, introCameraTarget_);
		EmitBossIntroDustBurst(settings_.dustStartBurstCount);
		EmitBossIntroImpactBurst(96, settings_.dustStartBurstCount);
		bossIntroStartDustDone_ = true;
		K4E::Log("[BossIntro] Spawn impact triggered.\n");
	}
	ChangeState(State::BossRising);
}

void BossIntroController::UpdateBossIntroDust(float deltaTime, float riseT)
{
	if (!settings_.enableBossIntroDust) return;
	if (!bossIntroStartDustDone_)
	{
		EmitBossIntroDustBurst(settings_.dustStartBurstCount);
		bossIntroStartDustDone_ = true;
	}
	bossIntroDustEmitTimer_ += deltaTime;
	if (bossIntroDustEmitTimer_ >= settings_.dustEmitInterval && riseT < 1.0f)
	{
		bossIntroDustEmitTimer_ = 0.0f;
		bossEnemyVfx_.UpdateAppearDust(MakeBossIntroDustPosition(), settings_.dustLoopEmitCount);
	}
}

void BossIntroController::EmitBossIntroDustBurst(uint32_t emitCount)
{
	if (!settings_.enableBossIntroDust || emitCount == 0) return;
	bossEnemyVfx_.UpdateAppearDust(MakeBossIntroDustPosition(), emitCount);
}

void BossIntroController::EmitBossIntroImpactBurst(uint32_t shockCount, uint32_t dustCount)
{
	const K4E::Vector3 position = MakeBossIntroDustPosition();
	BossAttackEffects::EmitGuardianHitEffect("BossIntroSpawnShockwave", K4E::GpuParticleType::Shockwave, position, shockCount, 3.2f, 0.9f, 1.7f);
	BossAttackEffects::EmitGuardianAttackPresenceEffect("BossIntroGroundBurst", K4E::GpuParticleType::Dust, position, dustCount, 2.6f, 0.75f, 1.4f);
	K4E::Vector3 pillarPosition = position;
	pillarPosition.y += 1.1f;
	BossAttackEffects::EmitGuardianAttackPresenceEffect("BossIntroRisingPillar", K4E::GpuParticleType::Trail, pillarPosition, std::max<uint32_t>(16, shockCount / 3), 1.35f, 1.0f, 1.2f);
}

K4E::Vector3 BossIntroController::MakeBossIntroDustPosition() const
{
	K4E::Vector3 dustPosition = settings_.bossAppearPosition;
	dustPosition.y = std::max(0.05f, dustPosition.y - settings_.dustGroundOffsetY);
	return dustPosition;
}

void BossIntroController::ResetBossIntroDustState()
{
	bossIntroDustEmitTimer_ = 0.0f;
	bossIntroStartDustDone_ = false;
	bossIntroEndDustDone_ = false;
	bossSpawnImpactTriggered_ = false;
	cameraShakeTimer_ = 0.0f;
	cameraShakeDuration_ = 0.0f;
	cameraShakeAmplitude_ = 0.0f;
	cameraShakeFrequency_ = 0.0f;
	cameraShakeSeed_ = 0.0f;
}

void BossIntroController::StartCameraShake(float duration, float amplitude, float frequency)
{
	if (!settings_.enableBossIntroCameraShake || duration <= 0.0f || amplitude <= 0.0f) return;
	cameraShakeDuration_ = duration;
	cameraShakeTimer_ = duration;
	cameraShakeAmplitude_ = amplitude;
	cameraShakeFrequency_ = frequency;
	cameraShakeSeed_ += 1.73f;
}

void BossIntroController::UpdateCameraShake(float deltaTime)
{
	if (cameraShakeTimer_ <= 0.0f) return;
	cameraShakeTimer_ = std::max(0.0f, cameraShakeTimer_ - deltaTime);
}

K4E::Vector3 BossIntroController::GetCameraShakeOffset() const
{
	if (cameraShakeTimer_ <= 0.0f || cameraShakeDuration_ <= 0.0f) return {};
	const float remainRate = Clamp01(cameraShakeTimer_ / cameraShakeDuration_);
	const float t = (cameraShakeDuration_ - cameraShakeTimer_) * cameraShakeFrequency_ + cameraShakeSeed_;
	const float amp = cameraShakeAmplitude_ * remainRate * remainRate;
	return { std::sin(t * 1.37f) * amp, std::cos(t * 1.91f) * amp * 0.55f, std::sin(t * 0.73f) * amp * 0.35f };
}

void BossIntroController::ApplyCameraLookAtBoss(K4E::Camera* camera, const K4E::Vector3& cameraPosition, const K4E::Vector3& targetPosition) const
{
	if (!camera) return;
	K4E::Vector3 direction = targetPosition - cameraPosition;
	if (K4E::Vector3::LengthSquared(direction) <= 0.000001f) direction = { 0.0f, 0.0f, 1.0f };
	camera->SetTranslate(cameraPosition + GetCameraShakeOffset());
	camera->SetForward(direction);
	camera->Update();
}

K4E::Vector3 BossIntroController::Lerp(const K4E::Vector3& a, const K4E::Vector3& b, float t)
{
	return a + (b - a) * Clamp01(t);
}

float BossIntroController::Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

const char* BossIntroController::ToStateLabel(State state)
{
	switch (state)
	{
	case State::WaitingAfterCrystalsBroken: return "WaitingAfterCrystalsBroken";
	case State::StartCutscene: return "StartCutscene";
	case State::CameraMoveToBoss: return "CameraMoveToBoss";
	case State::BossSpawnImpact: return "BossSpawnImpact";
	case State::BossRising: return "BossRising";
	case State::FinishCutscene: return "FinishCutscene";
	case State::Completed: return "Completed";
	default: return "None";
	}
}
