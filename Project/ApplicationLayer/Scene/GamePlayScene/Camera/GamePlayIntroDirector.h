#pragma once

#include "GamePlayStageContext.h"

#include <memory>
#include <vector>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	class Input;
}

class GamePlayFlow;
class GamePlayWorld;

class GamePlayIntroDirector
{
public: /// ---------- 型エイリアス ---------- ///

	using IntroCameraPointInfo = GamePlayStageContext::IntroCameraPointInfo;
	using IntroLookAtPointInfo = GamePlayStageContext::IntroLookAtPointInfo;

public: /// ---------- メンバ関数 ---------- ///

	void Reset(const GamePlayStageContext& stageContext, float introDuration = 2.5f);

	bool HasIntro() const { return !cameraPoints_.empty(); }

	void Update(
		float deltaTime,
		GamePlayFlow& flow,
		const GamePlayStageContext& stageContext,
		GamePlayWorld& world,
		K4E::Input* input,
		bool isDebugCamera);

private:
	void BeginGamePlayFromIntro(
		GamePlayFlow& flow,
		const GamePlayStageContext& stageContext,
		GamePlayWorld& world,
		K4E::Input* input,
		bool isDebugCamera);

private:
	std::vector<IntroCameraPointInfo> cameraPoints_;
	std::vector<IntroLookAtPointInfo> lookAtPoints_;

	float introTimer_ = 0.0f;
	float introDuration_ = 2.5f;
	int currentSegment_ = 0;
	float segmentTimer_ = 0.0f;
};