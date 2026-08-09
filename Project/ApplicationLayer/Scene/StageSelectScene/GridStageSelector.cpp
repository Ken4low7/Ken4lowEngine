#define NOMINMAX
#include "GameViewportConstants.h"
#include "GridStageSelector.h"
#include "DirectXCommon.h"
#include <GameTimer.h>
#include "Input.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <AudioManager.h>
#include <LinearInterpolation.h>

namespace K4E = ::Ken4lowEngine;

GridStageSelector::GridStageSelector()
{
	K4E::AudioManager::GetInstance()->PlaySE("negative.mp3", 0.0f, 1.0f, false); // ロック操作音を初回操作前にプリロードする。
}

void GridStageSelector::Initialize(const SelectorContext& context)
{
	context_ = context;
	stages_ = context_.stages;
	thumbs_.clear();
	lockUI_.sprites.clear();

	if (!stages_)
	{
		return;
	}

	for (const StageInfo& stage : *stages_)
	{
		auto sprite = std::make_unique<K4E::Sprite>();
		sprite->Initialize(stage.thumbPath.empty() ? "Effects/white.dds" : stage.thumbPath.c_str());
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		thumbs_.push_back(std::move(sprite));

		auto lock = std::make_unique<K4E::Sprite>();
		lock->Initialize("UI/Common/lock.dds");
		lock->SetAnchorPoint({ 0.5f, 0.5f });
		lock->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		lockUI_.sprites.push_back(std::move(lock));
	}

	selShadow_ = std::make_unique<K4E::Sprite>();
	selShadow_->Initialize("Effects/white.dds");
	selShadow_->SetAnchorPoint({ 0.5f, 0.5f });
	selShadow_->SetColor({ 0.0f, 0.0f, 0.0f, 0.38f });

	const float screenWidth = context_.screenWidth > 0.0f
		? context_.screenWidth
		: static_cast<float>(K4E::GameViewportConstants::Width);
	const float screenHeight = context_.screenHeight > 0.0f
		? context_.screenHeight
		: static_cast<float>(K4E::GameViewportConstants::Height);
	const float layoutScale = std::clamp(screenWidth / 1920.0f, 0.72f, 1.25f);
	layout_.center = { screenWidth * 0.5f, screenHeight * layout_.centerYRatio };
	layout_.baseW = 440.0f * layoutScale;
	layout_.baseH = 248.0f * layoutScale;
	layout_.gapX = 500.0f * layoutScale;

	scroll_.scrollX = 0.0f;
	scroll_.velocityX = 0.0f;
	selectionPulseTimer_ = 0.0f;
	unlockAnim_.timers.assign(stages_->size(), 0.0f);
	prevCenterIndex_ = GetCenterIndex();
	UpdateLayout();
	if (onCenterChanged_ && !thumbs_.empty())
	{
		onCenterChanged_(static_cast<uint32_t>(prevCenterIndex_));
	}
}

void GridStageSelector::Update(float deltaTime)
{
	if (!(deltaTime > 0.0f) || deltaTime > 0.05f)
	{
		deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();
	}
	if (!context_.input || thumbs_.empty())
	{
		return;
	}

	selectionPulseTimer_ += deltaTime;
	K4E::Vector2 mousePosition = context_.input->GetMousePosition();
	UpdatePress(context_.input, mousePosition);
	UpdateWheel();
	UpdateDrassing(context_.input, mousePosition, deltaTime);
	UpdateRelease(context_.input, mousePosition);
	UpdateTween(deltaTime);
	UpdateInertia(deltaTime);
	UpdateShake(deltaTime);
	CheckCenterCardChanged(deltaTime);
	UpdateLayout();
}

void GridStageSelector::Draw3DObjects()
{
}

void GridStageSelector::Draw2DSprites()
{
	if (selShadow_)
	{
		selShadow_->Draw();
	}

	const int selected = thumbs_.empty() ? -1 : GetCenterIndex();
	for (int i = 0; i < static_cast<int>(thumbs_.size()); ++i)
	{
		if (i != selected && thumbs_[i])
		{
			thumbs_[i]->Draw();
		}
	}
	if (selected >= 0 && selected < static_cast<int>(thumbs_.size()) && thumbs_[selected])
	{
		// 選択中カードを最後に描き、拡大時も隣のカードへ隠れないようにする。
		thumbs_[selected]->Draw();
	}

	for (int i = 0; i < static_cast<int>(lockUI_.sprites.size()); ++i)
	{
		if (i != selected && lockUI_.sprites[i])
		{
			lockUI_.sprites[i]->Draw();
		}
	}
	if (selected >= 0 && selected < static_cast<int>(lockUI_.sprites.size()) && lockUI_.sprites[selected])
	{
		lockUI_.sprites[selected]->Draw();
	}
}

void GridStageSelector::OnEnter()
{
	selectionPulseTimer_ = 0.0f;
}

void GridStageSelector::OnExit()
{
	CancelTween();
	scroll_.dragging = false;
	scroll_.pressIndex.reset();
	scroll_.velocityX = 0.0f;
	scroll_.lastDxPerSec = 0.0f;
}

void GridStageSelector::FocusToIndex(int index, bool tween)
{
	if (thumbs_.empty())
	{
		return;
	}
	index = std::clamp(index, 0, static_cast<int>(thumbs_.size()) - 1);
	if (tween)
	{
		StartTweenToIndex(index, 0.28f);
	}
	else
	{
		scroll_.scrollX = layout_.gapX * static_cast<float>(index);
		CancelTween();
		UpdateLayout();
	}
}

void GridStageSelector::PlayUnlockAnim(int index)
{
	if (index < 0 || index >= static_cast<int>(thumbs_.size()))
	{
		return;
	}
	if (unlockAnim_.timers.size() != thumbs_.size())
	{
		unlockAnim_.timers.assign(thumbs_.size(), 0.0f);
	}
	unlockAnim_.timers[index] = unlockAnim_.duration;
}

void GridStageSelector::UpdatePress(K4E::Input* input, K4E::Vector2& mousePosition)
{
	if (!input || !input->TriggerMouse(0))
	{
		return;
	}

	const int hit = HitTestCardIndex(mousePosition);
	if (hit >= 0)
	{
		scroll_.pressIndex = hit;
		scroll_.clickStartedOnCard = true;
	}
	else
	{
		scroll_.pressIndex.reset();
		scroll_.clickStartedOnCard = false;
	}

	scroll_.dragging = false;
	scroll_.dragStart = mousePosition;
	scroll_.lastMouse = mousePosition;
	scroll_.velocityX = 0.0f;
	scroll_.clickDeltaAccum = 0.0f;
	CancelTween();
}

void GridStageSelector::UpdateWheel()
{
	if (!context_.input || thumbs_.empty())
	{
		return;
	}

	const int wheel = context_.input->GetMouseWheel();
	if (wheel == 0 || scroll_.dragging || tween_.active)
	{
		return;
	}

	const int steps = std::clamp(wheel / 120, -3, 3);
	if (steps == 0)
	{
		return;
	}

	const int count = static_cast<int>(thumbs_.size());
	int targetIndex = GetCenterIndex() - steps;
	if (scroll_.loop)
	{
		targetIndex = (targetIndex % count + count) % count;
	}
	else
	{
		targetIndex = std::clamp(targetIndex, 0, count - 1);
	}
	StartTweenToIndex(targetIndex, 0.22f);
	scroll_.velocityX = 0.0f;
}

void GridStageSelector::UpdateDrassing(K4E::Input* input, K4E::Vector2& mousePosition, float deltaTime)
{
	if (!input || !scroll_.pressIndex.has_value() || !input->PushMouse(0))
	{
		return;
	}

	const float rawDx = mousePosition.x - scroll_.lastMouse.x;
	const float dx = std::clamp(rawDx, -tuning_.maxDxPerFrame, tuning_.maxDxPerFrame);
	const float maxX = layout_.gapX * std::max(0, static_cast<int>(thumbs_.size()) - 1);
	const bool over = !scroll_.loop && (scroll_.scrollX < 0.0f || scroll_.scrollX > maxX);
	const float dragFactor = over ? tuning_.overdragFactor : 1.0f;

	scroll_.dragging = true;
	scroll_.scrollX -= dx * dragFactor;
	scroll_.clickDeltaAccum += std::fabs(dx);
	scroll_.lastDxPerSec = dx / std::max(deltaTime, 0.0001f);
	scroll_.lastMouse = mousePosition;
}

void GridStageSelector::UpdateRelease(K4E::Input* input, K4E::Vector2& mousePosition)
{
	if (!input || !scroll_.pressIndex.has_value() || !input->ReleaseMouse(0))
	{
		return;
	}

	const int releaseHit = HitTestCardIndex(mousePosition);
	const bool click = scroll_.clickDeltaAccum < 8.0f;
	if (click && scroll_.clickStartedOnCard && releaseHit == *scroll_.pressIndex)
	{
		const int centerIndex = GetCenterIndex();
		if (*scroll_.pressIndex == centerIndex)
		{
			if (stages_ && (*stages_)[centerIndex].locked)
			{
				K4E::AudioManager::GetInstance()->PlaySE("negative02.mp3", 0.5f, 0.7f);
				TriggerLockedShake();
			}
			else if (context_.onRequestMap)
			{
				context_.onRequestMap(static_cast<uint32_t>(centerIndex));
			}
		}
		else
		{
			StartTweenToIndex(*scroll_.pressIndex, 0.28f);
			scroll_.velocityX = 0.0f;
		}
	}
	else
	{
		scroll_.velocityX = std::clamp(-scroll_.lastDxPerSec, -tuning_.maxVel, tuning_.maxVel);
	}

	scroll_.pressIndex.reset();
	scroll_.clickStartedOnCard = false;
	scroll_.dragging = false;
}

void GridStageSelector::UpdateTween(float deltaTime)
{
	if (!tween_.active)
	{
		return;
	}

	tween_.timer += deltaTime;
	const float t = std::clamp(tween_.timer / tween_.duration, 0.0f, 1.0f);
	const float eased = K4E::EaseInOutCubic(t);
	scroll_.scrollX = K4E::Lerp(tween_.startX, tween_.targetX, eased);
	if (t >= 1.0f)
	{
		CancelTween();
	}
}

void GridStageSelector::UpdateInertia(float deltaTime)
{
	if (tween_.active || scroll_.dragging || thumbs_.empty())
	{
		return;
	}

	const int count = static_cast<int>(thumbs_.size());
	const int index = GetCenterIndex();
	float target = layout_.gapX * static_cast<float>(index);
	if (scroll_.loop)
	{
		const float total = layout_.gapX * static_cast<float>(count);
		float current = std::fmod(scroll_.scrollX, total);
		if (current < 0.0f) current += total;
		float delta = target - current;
		if (delta > total * 0.5f) delta -= total;
		if (delta < -total * 0.5f) delta += total;
		target = scroll_.scrollX + delta;
	}

	scroll_.scrollX = K4E::Lerp(scroll_.scrollX, target, std::clamp(deltaTime * tuning_.snapK, 0.0f, 1.0f));
	if (!scroll_.loop)
	{
		const float maxX = layout_.gapX * std::max(0, count - 1);
		scroll_.scrollX = std::clamp(scroll_.scrollX, -80.0f, maxX + 80.0f);
	}
}

void GridStageSelector::UpdateLayout()
{
	const int count = static_cast<int>(thumbs_.size());
	if (count <= 0 || !stages_)
	{
		return;
	}

	const float wrappedScrollX = GetWrappedScrollX(count);
	const int selected = GetCenterIndex();
	for (int i = 0; i < count; ++i)
	{
		const float offsetX = GetCardOffsetX(i, count, wrappedScrollX);
		float centerX = layout_.center.x + offsetX;
		const float distanceFromCenter = std::fabs(offsetX);
		const float focusLinear = std::clamp(1.0f - distanceFromCenter / layout_.gapX, 0.0f, 1.0f);
		const float focus = focusLinear * focusLinear * (3.0f - 2.0f * focusLinear);
		float scale = 1.0f + focus * layout_.focusScale;

		if (!unlockAnim_.timers.empty() && i < static_cast<int>(unlockAnim_.timers.size()) && unlockAnim_.timers[i] > 0.0f)
		{
			const float unlockRate = std::clamp(unlockAnim_.timers[i] / std::max(0.01f, unlockAnim_.duration), 0.0f, 1.0f);
			scale *= 1.0f + 0.22f * std::sin(unlockRate * std::numbers::pi_v<float>);
		}
		if (i == selected)
		{
			scale *= 1.0f + std::sin(selectionPulseTimer_ * 2.4f) * 0.006f;
		}

		if (i == selected && (*stages_)[i].locked)
		{
			centerX += GetShakeOffsetX();
		}
		const float centerY = layout_.center.y + (1.0f - focus) * 10.0f;
		const bool locked = (*stages_)[i].locked;
		const float unlockedTint = K4E::Lerp(layout_.nonSelectedTint, 1.0f, focus);
		const float tint = locked ? K4E::Lerp(0.22f, 0.42f, focus) : unlockedTint;
		const float alpha = K4E::Lerp(layout_.edgeAlpha, 1.0f, focus);

		thumbs_[i]->SetPosition({ centerX, centerY });
		thumbs_[i]->SetSize({ layout_.baseW * scale, layout_.baseH * scale });
		thumbs_[i]->SetColor({ tint, tint, tint, alpha });
		thumbs_[i]->Update();

		if (i < static_cast<int>(lockUI_.sprites.size()) && lockUI_.sprites[i])
		{
			auto& icon = lockUI_.sprites[i];
			icon->SetPosition({ centerX + lockUI_.offset.x, centerY + lockUI_.offset.y });
			icon->SetSize({ layout_.baseW * scale * lockUI_.scale, layout_.baseH * scale * lockUI_.scale });
			icon->SetColor(locked
				? K4E::Vector4{ 1.0f, 1.0f, 1.0f, 0.72f + focus * 0.23f }
				: K4E::Vector4{ 1.0f, 1.0f, 1.0f, 0.0f });
			icon->Update();
		}
	}

	if (selShadow_ && selected >= 0 && selected < count)
	{
		const auto& selectedPosition = thumbs_[selected]->GetPosition();
		const auto& selectedSize = thumbs_[selected]->GetSize();
		selShadow_->SetPosition({ selectedPosition.x, selectedPosition.y + 10.0f });
		selShadow_->SetSize({ selectedSize.x * 1.08f, selectedSize.y * 1.10f });
		selShadow_->SetColor({ 0.0f, 0.0f, 0.0f, 0.42f });
		selShadow_->Update();
	}

	if (selected != prevCenterIndex_)
	{
		prevCenterIndex_ = selected;
		selectionPulseTimer_ = 0.0f;
		if (onCenterChanged_)
		{
			onCenterChanged_(static_cast<uint32_t>(selected));
		}
	}
}

void GridStageSelector::CheckCenterCardChanged(float deltaTime)
{
	for (float& timer : unlockAnim_.timers)
	{
		timer = std::max(0.0f, timer - deltaTime);
	}
}

int GridStageSelector::HitTestCardIndex(const K4E::Vector2& mousePosition) const
{
	const int count = static_cast<int>(thumbs_.size());
	if (count <= 0)
	{
		return -1;
	}

	const float wrappedScrollX = GetWrappedScrollX(count);
	for (int i = 0; i < count; ++i)
	{
		const float offsetX = GetCardOffsetX(i, count, wrappedScrollX);
		const float centerX = layout_.center.x + offsetX;
		const float centerY = layout_.center.y;
		const float focus = std::clamp(1.0f - std::fabs(offsetX) / layout_.gapX, 0.0f, 1.0f);
		const float scale = 1.0f + focus * layout_.focusScale;
		const float halfWidth = layout_.baseW * scale * 0.5f;
		const float halfHeight = layout_.baseH * scale * 0.5f;
		if (mousePosition.x >= centerX - halfWidth && mousePosition.x <= centerX + halfWidth &&
			mousePosition.y >= centerY - halfHeight && mousePosition.y <= centerY + halfHeight)
		{
			return i;
		}
	}
	return -1;
}

float GridStageSelector::GetWrappedScrollX(int cardCount) const
{
	if (!scroll_.loop || cardCount <= 0)
	{
		return scroll_.scrollX;
	}
	const float totalWidth = layout_.gapX * static_cast<float>(cardCount);
	float wrapped = std::fmod(scroll_.scrollX, totalWidth);
	return wrapped < 0.0f ? wrapped + totalWidth : wrapped;
}

float GridStageSelector::GetCardOffsetX(int index, int cardCount, float wrappedScrollX) const
{
	float offset = static_cast<float>(index) * layout_.gapX - wrappedScrollX;
	if (!scroll_.loop || cardCount <= 0)
	{
		return offset;
	}
	const float totalWidth = layout_.gapX * static_cast<float>(cardCount);
	const float halfWidth = totalWidth * 0.5f;
	if (offset > halfWidth) offset -= totalWidth;
	if (offset < -halfWidth) offset += totalWidth;
	return offset;
}

void GridStageSelector::StartTweenToIndex(int index, float duration)
{
	const int count = static_cast<int>(thumbs_.size());
	if (count <= 0)
	{
		return;
	}
	index = std::clamp(index, 0, count - 1);
	tween_.active = true;
	tween_.startX = scroll_.scrollX;
	float target = layout_.gapX * static_cast<float>(index);
	if (scroll_.loop)
	{
		const float total = layout_.gapX * static_cast<float>(count);
		float current = std::fmod(scroll_.scrollX, total);
		if (current < 0.0f) current += total;
		float delta = target - current;
		if (delta > total * 0.5f) delta -= total;
		if (delta < -total * 0.5f) delta += total;
		target = scroll_.scrollX + delta;
	}
	tween_.targetX = target;
	tween_.timer = 0.0f;
	tween_.duration = std::max(duration, 0.01f);
	scroll_.velocityX = 0.0f;
}

int GridStageSelector::GetCenterIndex() const
{
	const int count = static_cast<int>(thumbs_.size());
	if (count <= 0)
	{
		return 0;
	}
	const float wrappedScrollX = GetWrappedScrollX(count);
	int selected = 0;
	float bestDistance = std::numeric_limits<float>::max();
	for (int i = 0; i < count; ++i)
	{
		const float distance = std::fabs(GetCardOffsetX(i, count, wrappedScrollX));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			selected = i;
		}
	}
	return selected;
}

void GridStageSelector::TriggerLockedShake()
{
	shake_.active = true;
	shake_.timer = 0.0f;
}

void GridStageSelector::UpdateShake(float deltaTime)
{
	if (!shake_.active)
	{
		return;
	}
	shake_.timer += deltaTime;
	if (shake_.timer >= shake_.duration)
	{
		shake_.active = false;
		shake_.timer = 0.0f;
	}
}

float GridStageSelector::GetShakeOffsetX() const
{
	if (!shake_.active)
	{
		return 0.0f;
	}
	const float t = std::clamp(shake_.timer / shake_.duration, 0.0f, 1.0f);
	const float envelope = (1.0f - t) * (1.0f - t);
	const float phase = 2.0f * std::numbers::pi_v<float> * shake_.freqHz * shake_.timer;
	return std::sin(phase) * shake_.ampPx * envelope;
}
