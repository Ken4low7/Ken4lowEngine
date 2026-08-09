#pragma once

#include "IStageSelector.h"

#include <Actor.h>
#include <SceneComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// ステージセレクトの文字情報をTextComponent単位で所有するScreen Space UI Actor。
class StageSelectUIActor final : public K4E::Actor
{
public:
	void Initialize() override
	{
		const bool hasSavedLayout = FindComponentByName("Title") != nullptr;
		if (!GetRootComponent())
		{
			auto& root = CreateRootComponent<K4E::SceneComponent>();
			root.SetName("Stage Select UI Root");
		}

		title_ = EnsureTextComponent("Title", 100);
		stageNumber_ = EnsureTextComponent("Stage Number", 101);
		stageName_ = EnsureTextComponent("Stage Name", 102);
		category_ = EnsureTextComponent("Category", 103);
		objective_ = EnsureTextComponent("Objective", 104);
		description_ = EnsureTextComponent("Description", 105);
		unlockCondition_ = EnsureTextComponent("Unlock Condition", 106);
		guide_ = EnsureTextComponent("Guide", 107);

		if (title_) title_->SetText("ステージ選択");
		if (guide_) guide_->SetText("クリック：決定　ホイール／ドラッグ：移動　ESC：戻る");
		if (!hasSavedLayout) ApplyDefaultLayout(); // Prefab復元時は保存済みの位置と文字サイズを維持する。
		ApplyStageText();
		ApplyPresentation();
	}

	void Update(float deltaTime) override
	{
		if (ResolveTextComponents())
		{
			ApplyStageText(); // JSON再読込でComponent実体が変わった直後に動的なステージ文字を戻す。
		}
		const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
		transitionTimer_ = std::min(transitionDuration_, transitionTimer_ + safeDeltaTime);
		guidePulseTimer_ += safeDeltaTime;
		ApplyPresentation();
		K4E::Actor::Update(safeDeltaTime);
	}

	void UpdateEditor(float deltaTime) override
	{
		Update(deltaTime);
	}

	std::string GetClassTypeName() const override { return "StageSelectUIActor"; }

	void SetViewportSize(float width, float height)
	{
		const float safeWidth = std::max(1.0f, width);
		const float safeHeight = std::max(1.0f, height);
		if (std::fabs(viewportWidth_ - safeWidth) < 0.5f && std::fabs(viewportHeight_ - safeHeight) < 0.5f)
		{
			return;
		}
		viewportWidth_ = safeWidth;
		viewportHeight_ = safeHeight;
		ApplyDefaultLayout();
	}

	void SetStageInfo(const StageInfo& stage)
	{
		const bool changed = stageId_ != stage.id || stageNameText_ != stage.name || categoryText_ != stage.category ||
			descriptionText_ != stage.description || unlockText_ != stage.unlockCondition || locked_ != stage.locked;
		stageId_ = stage.id;
		stageNameText_ = stage.name;
		categoryText_ = stage.category;
		descriptionText_ = stage.description;
		unlockText_ = stage.unlockCondition;
		locked_ = stage.locked;
		if (changed) transitionTimer_ = 0.0f;
		ApplyStageText();
	}

private:
	K4E::TextComponent* EnsureTextComponent(const char* name, int drawOrder)
	{
		if (auto* existing = dynamic_cast<K4E::TextComponent*>(FindComponentByName(name)))
		{
			return existing;
		}
		auto& text = AddComponent<K4E::TextComponent>();
		text.SetName(name);
		text.SetDrawOrder(drawOrder);
		text.SetAnchor({ 0.5f, 0.5f });
		text.SetFontName("DotGothic16");
		return &text;
	}

	bool ResolveTextComponents()
	{
		const auto resolve = [this](const char* name) -> K4E::TextComponent*
		{
			return dynamic_cast<K4E::TextComponent*>(FindComponentByName(name));
		};
		K4E::TextComponent* newTitle = resolve("Title");
		K4E::TextComponent* newStageNumber = resolve("Stage Number");
		K4E::TextComponent* newStageName = resolve("Stage Name");
		K4E::TextComponent* newCategory = resolve("Category");
		K4E::TextComponent* newObjective = resolve("Objective");
		K4E::TextComponent* newDescription = resolve("Description");
		K4E::TextComponent* newUnlockCondition = resolve("Unlock Condition");
		K4E::TextComponent* newGuide = resolve("Guide");
		const bool changed = title_ != newTitle || stageNumber_ != newStageNumber || stageName_ != newStageName ||
			category_ != newCategory || objective_ != newObjective || description_ != newDescription ||
			unlockCondition_ != newUnlockCondition || guide_ != newGuide;
		title_ = newTitle;
		stageNumber_ = newStageNumber;
		stageName_ = newStageName;
		category_ = newCategory;
		objective_ = newObjective;
		description_ = newDescription;
		unlockCondition_ = newUnlockCondition;
		guide_ = newGuide;
		return changed;
	}

	void ApplyDefaultLayout()
	{
		const float scaleX = viewportWidth_ / 1920.0f;
		const float scaleY = viewportHeight_ / 1080.0f;
		const float uiScale = std::max(0.50f, std::min(scaleX, scaleY));
		const float centerX = viewportWidth_ * 0.5f;
		const auto setLayout = [centerX, scaleY, uiScale](K4E::TextComponent* component, float y, float fontSize)
		{
			if (!component) return;
			component->SetPosition({ centerX, y * scaleY });
			component->SetFontSize(fontSize * uiScale);
		};

		// サムネイル上端と下端を空け、見出しと説明を別々の情報帯として配置する。
		setLayout(title_, 64.0f, 48.0f);
		setLayout(stageNumber_, 322.0f, 26.0f);
		setLayout(stageName_, 734.0f, 40.0f);
		setLayout(category_, 790.0f, 28.0f);
		setLayout(objective_, 838.0f, 26.0f);
		setLayout(description_, 880.0f, 22.0f);
		setLayout(unlockCondition_, 926.0f, 21.0f);
		setLayout(guide_, 1032.0f, 18.0f);
	}

	void ApplyStageText()
	{
		char stageNumberText[32]{};
		std::snprintf(stageNumberText, sizeof(stageNumberText), "ステージ %02u", stageId_ + 1u);
		if (title_) title_->SetText("ステージ選択");
		if (stageNumber_) stageNumber_->SetText(stageNumberText);
		if (stageName_) stageName_->SetText(stageNameText_);
		if (category_) category_->SetText(GetCategoryDisplayName(categoryText_));
		if (objective_) objective_->SetText(std::string("目標：") + GetCategoryObjective(categoryText_));
		if (description_) description_->SetText(descriptionText_);
		if (unlockCondition_)
		{
			unlockCondition_->SetVisible(locked_);
			unlockCondition_->SetText(GetUnlockDisplayText());
		}
		if (guide_) guide_->SetText("クリック：決定　ホイール／ドラッグ：移動　ESC：戻る");
	}

	void ApplyPresentation()
	{
		const float stageAlpha = BuildRevealAlpha(0.00f, 0.18f);
		const float categoryAlpha = BuildRevealAlpha(0.08f, 0.18f);
		const float objectiveAlpha = BuildRevealAlpha(0.16f, 0.18f);
		const float descriptionAlpha = BuildRevealAlpha(0.24f, 0.18f);
		const float unlockAlpha = BuildRevealAlpha(0.30f, 0.16f);
		const float pulse = 0.5f + 0.5f * std::sin(guidePulseTimer_ * 2.0f);
		const K4E::Vector4 accent = GetCategoryAccentColor(categoryText_);
		const float categoryFlash = 1.0f + (1.0f - categoryAlpha) * 0.28f;

		if (title_) title_->SetColor({ 1.0f, 1.0f, 1.0f, 0.96f });
		if (stageNumber_) stageNumber_->SetColor({ 0.86f, 0.94f, 1.0f, stageAlpha });
		if (stageName_) stageName_->SetColor({ 1.0f, 1.0f, 1.0f, stageAlpha });
		if (category_) category_->SetColor({ std::min(1.0f, accent.x * categoryFlash), std::min(1.0f, accent.y * categoryFlash), std::min(1.0f, accent.z * categoryFlash), categoryAlpha });
		if (objective_) objective_->SetColor({ 1.0f, 0.96f, 0.80f, objectiveAlpha });
		if (description_) description_->SetColor({ 0.88f, 0.93f, 0.96f, descriptionAlpha });
		if (unlockCondition_) unlockCondition_->SetColor({ 1.0f, 0.72f, 0.72f, unlockAlpha });
		if (guide_) guide_->SetColor({ 1.0f, 1.0f, 1.0f, 0.68f + pulse * 0.25f });
	}

	float BuildRevealAlpha(float delay, float duration) const
	{
		const float localTime = std::max(0.0f, transitionTimer_ - delay);
		const float normalized = duration > 0.0f ? std::clamp(localTime / duration, 0.0f, 1.0f) : 1.0f;
		return 1.0f - std::pow(1.0f - normalized, 3.0f); // 情報を同時表示せず、任務種別から説明へ順番に読み取れるようにする。
	}

	std::string GetUnlockDisplayText() const
	{
		if (!locked_) return {};
		char unlockText[64]{};
		std::snprintf(unlockText, sizeof(unlockText), "ステージ%02uをクリアすると開放", stageId_);
		return unlockText; // 保存済みの英語表記に依存せず、画面上の開放条件を日本語へ統一する。
	}

	static const char* GetCategoryDisplayName(const std::string& category)
	{
		if (category == "WAVE") return "ウェーブ戦";
		if (category == "SEARCH") return "探索任務";
		if (category == "DEFENSE") return "防衛任務";
		if (category == "ESCAPE") return "脱出任務";
		if (category == "BOSS") return "最終決戦";
		return "任務情報なし";
	}

	static const char* GetCategoryObjective(const std::string& category)
	{
		if (category == "WAVE") return "敵の波を退け、戦況を切り開け";
		if (category == "SEARCH") return "坑道を探索し、進路を開放せよ";
		if (category == "DEFENSE") return "押し寄せる敵から拠点を守り抜け";
		if (category == "ESCAPE") return "敵の包囲を突破し、出口へ到達せよ";
		if (category == "BOSS") return "すべてを懸けて最終ボスを撃破せよ";
		return "ステージ目標を達成せよ";
	}

	static K4E::Vector4 GetCategoryAccentColor(const std::string& category)
	{
		if (category == "WAVE") return { 0.80f, 0.96f, 1.00f, 1.0f };
		if (category == "SEARCH") return { 1.00f, 0.90f, 0.60f, 1.0f };
		if (category == "DEFENSE") return { 0.68f, 0.88f, 1.00f, 1.0f };
		if (category == "ESCAPE") return { 1.00f, 0.76f, 0.62f, 1.0f };
		if (category == "BOSS") return { 1.00f, 0.55f, 0.55f, 1.0f };
		return { 1.0f, 1.0f, 1.0f, 1.0f };
	}

private:
	K4E::TextComponent* title_ = nullptr;
	K4E::TextComponent* stageNumber_ = nullptr;
	K4E::TextComponent* stageName_ = nullptr;
	K4E::TextComponent* category_ = nullptr;
	K4E::TextComponent* objective_ = nullptr;
	K4E::TextComponent* description_ = nullptr;
	K4E::TextComponent* unlockCondition_ = nullptr;
	K4E::TextComponent* guide_ = nullptr;

	float viewportWidth_ = 1920.0f;
	float viewportHeight_ = 1080.0f;
	float transitionTimer_ = 0.0f;
	float transitionDuration_ = 0.46f;
	float guidePulseTimer_ = 0.0f;
	std::uint32_t stageId_ = 0u;
	std::string stageNameText_ = "始まりの平原";
	std::string categoryText_ = "WAVE";
	std::string descriptionText_ = "基本戦闘を学ぶウェーブ制ステージ";
	std::string unlockText_;
	bool locked_ = false;
};
