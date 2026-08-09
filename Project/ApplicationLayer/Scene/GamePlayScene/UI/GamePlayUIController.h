#pragma once

#include "FadeManager.h"
#include "GamePlayFlow.h"
#include "GamePlayWorld.h"

#include <SpriteManager.h>

/// <summary>
/// GamePlayScene の 2D UI 描画順を管理する軽量 Controller です。<br/>
/// Scene 本体から HUD / Flow UI / Fade の描画詳細を切り離し、
/// Scene は「いつ描くか」だけを決められるようにします。
/// </summary>
class GamePlayUIController
{
public:
	/// <summary>
	/// HUD、ポーズ/リザルトUI、Scene内フェードを既存順序のまま描画します。
	/// </summary>
	void Draw2DSprites(GamePlayWorld* world, GamePlayFlow* flow, FadeManager* fadeManager, bool hideGameplayUI)
	{
		Ken4lowEngine::SpriteManager::GetInstance()->SetRenderSetting_Background();
		Ken4lowEngine::SpriteManager::GetInstance()->SetRenderSetting_UI();

		if (world)
		{
			if (!hideGameplayUI) world->EnsureBossStageStarted(); // Stage紹介カメラと重ならないよう、操作開始可能になった最初のHUDフレームでBossを呼ぶ。
			if (HUDManager* hud = world->GetHUDManager())
			{
				HUDManager::StageObjectiveDisplayState displayState{};
				if (const StageObjectiveManager::Snapshot* snapshot = world->GetStageObjectiveSnapshot())
				{
					// Stage1は専用ガイドを使い、Stage2以降だけ共通Objective HUDへ表示する。
					displayState.visible = snapshot->type != GamePlayStageContext::StageObjectiveType::ClearAllWaves;
					displayState.showProgress = snapshot->usesCount || snapshot->usesTimer;
					displayState.cleared = snapshot->status == StageObjectiveManager::Status::Cleared;
					displayState.failed = snapshot->status == StageObjectiveManager::Status::Failed;
					displayState.title = snapshot->title;
					displayState.detail = snapshot->detail;
					displayState.normalizedProgress = snapshot->normalizedProgress;
				}
				hud->SetStageObjectiveDisplayState(displayState);
			}

			// HUD は World が所有するため、表示抑制条件だけ Scene 側から渡す。
			world->DrawHUD(hideGameplayUI);
		}

		if (flow && !hideGameplayUI)
		{
			flow->DrawUI();
		}

		if (fadeManager)
		{
			fadeManager->Draw2DSprites();
		}
	}
};
