#define NOMINMAX
#include "CrystalHpBarController.h"

#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "EnemyHPBar.h"
#include "EnemyHPBarProjector.h"
#include "FontAtlasLoader.h"
#include "TextSpriteDrawer.h"
#include "Sprite.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace
{
	float Length(const K4E::Vector3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	K4E::Vector4 ResolveHpTextColor(float hpRate)
	{
		if (hpRate <= 0.20f) return { 1.0f, 0.30f, 0.18f, 1.0f };
		if (hpRate <= 0.45f) return { 1.0f, 0.78f, 0.20f, 1.0f };
		return { 0.48f, 0.92f, 1.0f, 1.0f };
	}
}

CrystalHpBarController::~CrystalHpBarController()
{
	Finalize();
}

void CrystalHpBarController::Initialize(size_t crystalCount)
{
	Finalize();
	InitializeTextRenderer();
	EnsureBarCount(crystalCount);
}

void CrystalHpBarController::Finalize()
{
	hpBars_.clear();
	directionMarkers_.clear();
	textDrawer_.reset();
	debugInfos_.clear();
	aimTimers_.clear();
	textReady_ = false;
	drawCalled_ = false;
	visibleCount_ = 0;
	previousAliveCrystalCount_ = -1;
	objectiveNoticeTimer_ = 0.0f;
	markerPulseTimer_ = 0.0f;
	objectiveNoticeText_.clear();
}

void CrystalHpBarController::Update(const std::vector<EnemySpawnCrystal>& crystals, const K4E::Matrix4x4& viewMatrix, const K4E::Matrix4x4& projMatrix, float screenWidth, float screenHeight, float deltaTime, const EnemySpawnCrystal* aimedCrystal, bool showOnlyWhenAimed, float visibleHoldTime)
{
	drawCalled_ = false;
	visibleCount_ = 0;
	screenWidth_ = std::max(1.0f, screenWidth);
	screenHeight_ = std::max(1.0f, screenHeight);
	markerPulseTimer_ += std::max(0.0f, deltaTime);
	EnsureBarCount(crystals.size());
	UpdateObjectiveNotice(crystals, deltaTime);

	const float displayWidth = std::max(140.0f, settings_.width);
	const float displayHeight = std::max(14.0f, settings_.height);
	const IPlayerRuntime* player = IPlayerRuntime::GetActiveRuntimeConst();
	const bool hasPlayerPosition = player != nullptr;
	const K4E::Vector3 playerPosition = hasPlayerPosition ? player->GetWorldPosition() : K4E::Vector3{};

	for (size_t i = 0; i < crystals.size(); ++i)
	{
		const EnemySpawnCrystal& crystal = crystals[i];
		const bool availableForUi = crystal.IsAlive() && !crystal.IsBreaking();
		DebugInfo debug{};
		debug.hp = crystal.GetHp();
		debug.maxHp = crystal.GetMaxHp();
		debug.hpRate = std::clamp(crystal.GetHpRate(), 0.0f, 1.0f);
		debug.active = availableForUi;
		debug.broken = crystal.IsDestroyed();
		debug.distance = hasPlayerPosition ? Length(crystal.GetPosition() - playerPosition) : 0.0f;

		bool visible = settings_.visible;
		if (&crystal == aimedCrystal)
		{
			aimTimers_[i] = std::max(0.0f, visibleHoldTime);
		}
		else if (aimTimers_[i] > 0.0f)
		{
			aimTimers_[i] = std::max(0.0f, aimTimers_[i] - deltaTime);
		}

		if (!availableForUi)
		{
			visible = false;
			debug.hiddenReason = crystal.IsBreaking() ? "Breaking" : "Broken";
		}
		else if (showOnlyWhenAimed && !settings_.alwaysVisible && &crystal != aimedCrystal && aimTimers_[i] <= 0.0f)
		{
			// 常時表示設定を優先し、照準表示モードでは外れ際だけ短く保持する。
			visible = false;
			debug.hiddenReason = "Not aimed";
		}
		else if (!settings_.alwaysVisible && crystal.GetHp() >= crystal.GetMaxHp() && &crystal != aimedCrystal)
		{
			visible = false;
			debug.hiddenReason = "Full HP";
		}

		const K4E::Vector3& pos = crystal.GetPosition();
		const K4E::Vector3& scale = crystal.GetScale();
		const K4E::Vector3 hpBarWorldPos{ pos.x, pos.y + std::abs(scale.y) * 0.65f + settings_.offsetY, pos.z };
		debug.worldPosition = hpBarWorldPos;
		const HpBarProjectResult projected = ProjectWorldToScreen(hpBarWorldPos, viewMatrix, projMatrix, screenWidth_, screenHeight_);
		debug.screenPosition = projected.screenPos;
		debug.inFront = projected.inFront;
		debug.inScreen = projected.inScreen;
		if (visible && (!projected.inFront || !projected.inScreen))
		{
			visible = false;
			debug.hiddenReason = projected.inFront ? "Out of screen" : "Behind camera";
		}

		debug.visible = visible;
		debug.hpLabelVisible = visible;
		debug.hpLabelPosition = { projected.screenPos.x, projected.screenPos.y - displayHeight * 0.95f - 9.0f };
		if (visible) ++visibleCount_;

		if (i < hpBars_.size() && hpBars_[i])
		{
			hpBars_[i]->Update(projected.screenPos, debug.hpRate, visible, deltaTime, displayWidth, displayHeight);
		}

		debug.markerVisible = settings_.showOffscreenMarker && availableForUi && (!projected.inFront || !projected.inScreen);
		if (debug.markerVisible)
		{
			debug.markerPosition = BuildOffscreenMarkerPosition(hpBarWorldPos, viewMatrix, screenWidth_, screenHeight_);
			debug.distanceLabelPosition = { debug.markerPosition.x, debug.markerPosition.y + settings_.markerSize * 0.85f };
		}
		else if (availableForUi && projected.inFront && projected.inScreen)
		{
			debug.distanceLabelPosition = { projected.screenPos.x, projected.screenPos.y + displayHeight + 18.0f };
		}
		debug.distanceVisible = settings_.showDistance && hasPlayerPosition && availableForUi && (debug.markerVisible || (projected.inFront && projected.inScreen));

		if (i < directionMarkers_.size() && directionMarkers_[i])
		{
			auto& marker = directionMarkers_[i];
			if (debug.markerVisible)
			{
				const float pulse = 0.5f + 0.5f * std::sin(markerPulseTimer_ * 5.0f);
				const bool aimed = &crystal == aimedCrystal;
				const K4E::Vector4 color = aimed
					? K4E::Vector4{ 1.0f, 0.88f, 0.18f, 0.92f }
					: K4E::Vector4{ 0.20f, 0.86f, 1.0f, 0.72f + pulse * 0.24f };
				marker->SetPosition(debug.markerPosition);
				marker->SetSize({ settings_.markerSize * (0.88f + pulse * 0.12f), settings_.markerSize * (0.88f + pulse * 0.12f) });
				marker->SetRotation(std::numbers::pi_v<float> * 0.25f);
				marker->SetColor(color);
			}
			else
			{
				marker->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			}
			marker->Update();
		}

		debugInfos_[i] = debug;
	}
}

void CrystalHpBarController::Draw()
{
	drawCalled_ = true;
	for (auto& bar : hpBars_)
	{
		if (bar) bar->Draw();
	}
	for (size_t i = 0; i < directionMarkers_.size() && i < debugInfos_.size(); ++i)
	{
		if (directionMarkers_[i] && debugInfos_[i].markerVisible) directionMarkers_[i]->Draw();
	}

	if (!textReady_ || !textDrawer_) return;

	textDrawer_->Reset();
	textDrawer_->SetLetterSpacing(0.5f);
	textDrawer_->SetLineSpacing(2.0f);

	for (const DebugInfo& info : debugInfos_)
	{
		if (!info.hpLabelVisible) continue;
		const std::string hpText = "CRYSTAL  " + std::to_string(info.hp) + " / " + std::to_string(info.maxHp);
		textDrawer_->SetScale(settings_.hpTextScale);
		textDrawer_->SetColor({ 0.01f, 0.02f, 0.04f, 0.94f });
		textDrawer_->DrawTextCentered(hpText, { info.hpLabelPosition.x + 2.0f, info.hpLabelPosition.y + 2.0f });
		textDrawer_->SetColor(ResolveHpTextColor(info.hpRate));
		textDrawer_->DrawTextCentered(hpText, info.hpLabelPosition); // 数値と対象名を併記し、ゲージだけより残量を読み取りやすくする。
	}

	for (const DebugInfo& info : debugInfos_)
	{
		if (!info.distanceVisible) continue;
		const std::string distanceText = std::to_string(static_cast<int>(std::round(info.distance))) + "m";
		textDrawer_->SetScale(settings_.distanceTextScale);
		textDrawer_->SetColor({ 0.01f, 0.02f, 0.04f, 0.90f });
		textDrawer_->DrawTextCentered(distanceText, { info.distanceLabelPosition.x + 2.0f, info.distanceLabelPosition.y + 2.0f });
		textDrawer_->SetColor(info.markerVisible
			? K4E::Vector4{ 0.55f, 0.94f, 1.0f, 1.0f }
			: K4E::Vector4{ 0.82f, 0.96f, 1.0f, 0.95f });
		textDrawer_->DrawTextCentered(distanceText, info.distanceLabelPosition);
	}

	if (objectiveNoticeTimer_ > 0.0f && !objectiveNoticeText_.empty())
	{
		const float alpha = std::clamp(objectiveNoticeTimer_ / 0.35f, 0.0f, 1.0f);
		const K4E::Vector2 noticePosition{ screenWidth_ * 0.5f, 118.0f };
		textDrawer_->SetScale(0.76f);
		textDrawer_->SetColor({ 0.01f, 0.02f, 0.04f, alpha * 0.92f });
		textDrawer_->DrawTextCentered(objectiveNoticeText_, { noticePosition.x + 3.0f, noticePosition.y + 3.0f });
		textDrawer_->SetColor({ 1.0f, 0.86f, 0.22f, alpha });
		textDrawer_->DrawTextCentered(objectiveNoticeText_, noticePosition);
	}
}

void CrystalHpBarController::DrawImGui() const
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Crystal HP Bar / Navigation");
	ImGui::Text("表示: %s", settings_.visible ? "ON" : "OFF");
	ImGui::Text("常時表示: %s", settings_.alwaysVisible ? "ON" : "OFF");
	ImGui::Text("距離 / 画面外マーカー: %s / %s", settings_.showDistance ? "ON" : "OFF", settings_.showOffscreenMarker ? "ON" : "OFF");
	ImGui::Text("OffsetY / Size: %.2f / %.1f x %.1f", settings_.offsetY, std::max(140.0f, settings_.width), std::max(14.0f, settings_.height));
	ImGui::Text("Marker Margin / Size: %.1f / %.1f", settings_.markerMargin, settings_.markerSize);
	ImGui::Text("被弾後表示時間: %.2f 秒", settings_.showTime);
	ImGui::Text("Draw呼び出し: %s", drawCalled_ ? "はい" : "いいえ");
	ImGui::Text("表示対象数: %d / %d", visibleCount_, static_cast<int>(debugInfos_.size()));
	for (size_t i = 0; i < debugInfos_.size(); ++i)
	{
		const auto& info = debugInfos_[i];
		ImGui::Text(
			"Crystal[%d] HP:%d/%d Dist:%.1fm Screen:(%.1f,%.1f) marker:%s visible:%s reason:%s",
			static_cast<int>(i),
			info.hp,
			info.maxHp,
			info.distance,
			info.screenPosition.x,
			info.screenPosition.y,
			info.markerVisible ? "true" : "false",
			info.visible ? "true" : "false",
			info.hiddenReason.empty() ? "-" : info.hiddenReason.c_str());
	}
#endif
}

void CrystalHpBarController::EnsureBarCount(size_t crystalCount)
{
	if (debugInfos_.size() != crystalCount) debugInfos_.resize(crystalCount);
	if (aimTimers_.size() != crystalCount) aimTimers_.assign(crystalCount, 0.0f);
	if (hpBars_.size() == crystalCount && directionMarkers_.size() == crystalCount) return;

	hpBars_.clear();
	directionMarkers_.clear();
	hpBars_.reserve(crystalCount);
	directionMarkers_.reserve(crystalCount);
	for (size_t i = 0; i < crystalCount; ++i)
	{
		auto bar = std::make_unique<EnemyHPBar>();
		bar->Initialize();
		hpBars_.push_back(std::move(bar));

		auto marker = std::make_unique<K4E::Sprite>();
		marker->Initialize("Effects/white.dds");
		marker->SetAnchorPoint({ 0.5f, 0.5f });
		marker->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		directionMarkers_.push_back(std::move(marker));
	}
	aimTimers_.assign(crystalCount, 0.0f);
}

void CrystalHpBarController::InitializeTextRenderer()
{
	textDrawer_ = std::make_unique<K4E::TextSpriteDrawer>();
	textReady_ = false;
	try
	{
		auto fontDef = K4E::FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?');
		textDrawer_->Initialize(fontDef);
		textReady_ = true;
	}
	catch (...)
	{
		textDrawer_.reset();
		textReady_ = false;
	}
}

void CrystalHpBarController::UpdateObjectiveNotice(const std::vector<EnemySpawnCrystal>& crystals, float deltaTime)
{
	objectiveNoticeTimer_ = std::max(0.0f, objectiveNoticeTimer_ - std::max(0.0f, deltaTime));
	const int aliveCrystalCount = static_cast<int>(std::count_if(
		crystals.begin(),
		crystals.end(),
		[](const EnemySpawnCrystal& crystal) { return crystal.IsAlive(); }));

	if (previousAliveCrystalCount_ < 0)
	{
		previousAliveCrystalCount_ = aliveCrystalCount;
		return;
	}

	if (aliveCrystalCount < previousAliveCrystalCount_)
	{
		objectiveNoticeText_ = aliveCrystalCount > 0
			? "目標更新：クリスタル 残り " + std::to_string(aliveCrystalCount) + " 個"
			: "目標更新：全クリスタル破壊";
		objectiveNoticeTimer_ = std::max(0.1f, settings_.objectiveNoticeTime);
	}
	previousAliveCrystalCount_ = aliveCrystalCount;
}

K4E::Vector2 CrystalHpBarController::BuildOffscreenMarkerPosition(const K4E::Vector3& worldPosition, const K4E::Matrix4x4& viewMatrix, float screenWidth, float screenHeight) const
{
	const K4E::Vector3 viewPosition = K4E::Vector3::Transform(worldPosition, viewMatrix);
	float directionX = viewPosition.x;
	float directionY = -viewPosition.y;
	if (viewPosition.z <= 0.01f)
	{
		directionX = -directionX;
		directionY = -directionY;
	}

	const float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
	if (directionLength <= 0.0001f)
	{
		directionX = 0.0f;
		directionY = -1.0f;
	}
	else
	{
		directionX /= directionLength;
		directionY /= directionLength;
	}

	const float centerX = screenWidth * 0.5f;
	const float centerY = screenHeight * 0.5f;
	const float halfWidth = std::max(1.0f, centerX - settings_.markerMargin);
	const float halfHeight = std::max(1.0f, centerY - settings_.markerMargin);
	const float scaleX = std::fabs(directionX) > 0.0001f ? halfWidth / std::fabs(directionX) : 1.0e6f;
	const float scaleY = std::fabs(directionY) > 0.0001f ? halfHeight / std::fabs(directionY) : 1.0e6f;
	const float edgeScale = std::min(scaleX, scaleY);

	// 画面外のクリスタルは安全領域の端へ固定し、位置と距離を同時に示す。
	return { centerX + directionX * edgeScale, centerY + directionY * edgeScale };
}
