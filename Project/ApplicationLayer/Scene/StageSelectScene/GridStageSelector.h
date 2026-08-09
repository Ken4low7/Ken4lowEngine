#pragma once
#include "IStageSelector.h"
#include <Sprite.h>
#include <Vector2.h>

#include <optional>
#include <memory>
#include <limits>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///　　　　　　　グリッド型ステージセレクター
/// -------------------------------------------------------------
class GridStageSelector : public IStageSelector
{
private: /// ---------- 構造体 ---------- ///

	struct LockIconSet
	{
		std::vector<std::unique_ptr<K4E::Sprite>> sprites;
		float scale = 0.34f;
		K4E::Vector2 offset = { 0.0f, 0.0f };
	};

	struct LayoutParam
	{
		K4E::Vector2 center = { 960.0f, 510.0f };
		float gapX = 500.0f;
		float baseW = 440.0f;
		float baseH = 248.0f;
		float focusScale = 0.24f;
		float nonSelectedTint = 0.52f;
		float edgeAlpha = 0.74f;
		float centerYRatio = 0.47f;
	};

	struct ScrollDragState
	{
		float scrollX = 0.0f;
		float velocityX = 0.0f;
		bool dragging = false;
		K4E::Vector2 lastMouse{};
		K4E::Vector2 dragStart{};
		float clickDeltaAccum = 0.0f;
		std::optional<int> pressIndex;
		bool clickStartedOnCard = false;
		float lastDxPerSec = 0.0f;
		bool loop = true;
	};

	struct TuningParam
	{
		float maxVel = 2000.0f;
		float maxDxPerFrame = 100.0f;
		float overdragFactor = 0.5f;
		float snapK = 8.0f;
	};

	struct TweenState
	{
		bool active = false;
		float startX = 0.0f;
		float targetX = 0.0f;
		float timer = 0.0f;
		float duration = 0.3f;
	};

	struct ShakeState
	{
		bool active = false;
		float timer = 0.0f;
		float duration = 0.28f;
		float ampPx = 18.0f;
		float freqHz = 28.0f;
	};

	struct UnlockAnimState
	{
		std::vector<float> timers;
		float duration = 0.6f;
	};

public:
	virtual ~GridStageSelector() = default;
	GridStageSelector();
	void Initialize(const SelectorContext& context) override;
	void Update(float deltaTime) override;
	void Draw3DObjects() override;
	void Draw2DSprites() override;
	void OnEnter() override;
	void OnExit() override;
	void FocusToIndex(int index, bool tween = true) override;
	void SetOnCenterChanged(std::function<void(uint32_t)> callback) { onCenterChanged_ = callback; }
	void PlayUnlockAnim(int index);
	void PlayUnlockPresentation(int index)
	{
		TriggerLockedShake();
		PlayUnlockAnim(index); // 鍵付きカードの揺れと拡大を同じ解除演出として開始する。
	}

private:
	void UpdatePress(K4E::Input* input, K4E::Vector2& mp);
	void UpdateWheel();
	void UpdateDrassing(K4E::Input* input, K4E::Vector2& mp, float deltaTime);
	void UpdateRelease(K4E::Input* input, K4E::Vector2& mp);
	void UpdateTween(float deltaTime);
	void UpdateInertia(float deltaTime);
	void UpdateLayout();
	void CheckCenterCardChanged(float deltaTime);
	int HitTestCardIndex(const K4E::Vector2& mousePosition) const;
	float GetWrappedScrollX(int cardCount) const;
	float GetCardOffsetX(int index, int cardCount, float wrappedScrollX) const;
	void StartTweenToIndex(int index, float duration = 0.3f);
	void CancelTween() { tween_.active = false; tween_.timer = 0.0f; }
	int GetCenterIndex() const;
	void TriggerLockedShake();
	void UpdateShake(float dt);
	float GetShakeOffsetX() const;

private:
	SelectorContext context_{};
	const std::vector<StageInfo>* stages_ = nullptr;
	std::vector<std::unique_ptr<K4E::Sprite>> thumbs_;
	std::unique_ptr<K4E::Sprite> selShadow_;
	LockIconSet lockUI_{};
	std::function<void(uint32_t)> onCenterChanged_;
	int prevCenterIndex_ = -1;
	LayoutParam layout_{};
	ScrollDragState scroll_{};
	TuningParam tuning_{};
	TweenState tween_{};
	ShakeState shake_{};
	UnlockAnimState unlockAnim_{};
	float selectionPulseTimer_ = 0.0f;
};
