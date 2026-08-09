#define NOMINMAX
#include "StageSelectScene.h"

#include "GameViewportConstants.h"
#include "SceneManager.h"
#include "StageRepository.h"
#include "StageSelectSelectingState.h"
#include "StageSelectUIActor.h"

#include <CameraManager.h>
#include <DirectXCommon.h>
#include <Editor/EditorTransformAccess.h>
#include <GameTimer.h>
#include <Input.h>
#include <SpriteManager.h>

#include <algorithm>
#include <utility>

namespace K4E = ::Ken4lowEngine;

void StageSelectScene::Initialize()
{
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	input_ = K4E::Input::GetInstance();

	stages_.clear();
	context_ = {};
	gridSelector_.reset();
	activeSelector_ = nullptr;
	bg_.reset();
	currentState_.reset();

	uiActorWorld_.Finalize();
	uiActorWorld_.Initialize();
	stageSelectUIActor_ = nullptr;
	syncedUiStageIndex_ = -1;

	pendingUnlockIndex_ = -1;
	nextScene_ = NextScene::None;
	loadStep_ = 0;
	isLoadReady_ = false;
	currentStageIndex_ = 0;
	state_ = State::Loading;
}

void StageSelectScene::Update()
{
	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();
	if (state_ == State::Loading)
	{
		if (bg_) bg_->Update();
		return;
	}

	if (currentState_)
	{
		currentState_->Update(this, deltaTime);
	}

	SyncStageSelectUI();
	uiActorWorld_.Update(deltaTime);
}

void StageSelectScene::UpdateEditor(float deltaTime)
{
	if (bg_) bg_->Update();
	SyncStageSelectUI();
	uiActorWorld_.UpdateEditor(deltaTime);
}

void StageSelectScene::Draw3DObjects()
{
}

void StageSelectScene::DrawShadowObjects()
{
}

void StageSelectScene::Draw2DSprites()
{
	K4E::SpriteManager::GetInstance()->SetRenderSetting_Background();
	if (bg_) bg_->Draw();

	K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
	if (activeSelector_) activeSelector_->Draw2DSprites();
	uiActorWorld_.DrawScreenSpaceUI();
}

void StageSelectScene::Finalize()
{
	if (currentState_)
	{
		currentState_->Exit(this);
		currentState_.reset();
	}
	if (activeSelector_)
	{
		activeSelector_->OnExit();
	}

	activeSelector_ = nullptr;
	gridSelector_.reset();
	stageSelectUIActor_ = nullptr;
	syncedUiStageIndex_ = -1;
	uiActorWorld_.Finalize();

	bg_.reset();
	stages_.clear();
	pendingUnlockIndex_ = -1;
	nextScene_ = NextScene::None;
	state_ = State::Selecting;
	loadStep_ = 0;
	isLoadReady_ = false;
	input_ = nullptr;
	dxCommon_ = nullptr;
}

void StageSelectScene::DrawImGui()
{
	uiActorWorld_.DrawImGui();
}

void StageSelectScene::CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects)
{
	if (bg_)
	{
		// 背景SpriteだけはScene所有のため、Actor UIとは別の編集対象として公開する。
		outObjects.push_back(Ken4lowEngine::MakeSpriteEditorObject(
			0x5354414745424701ull,
			"Stage Select Background",
			"Sprite",
			"StageSelectScene",
			bg_.get()));
	}
}

void StageSelectScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
	state_ = State::Loading;
}

void StageSelectScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		InitializeStages();
		++loadStep_;
		break;
	case 1:
		InitializeSelectors();
		++loadStep_;
		break;
	case 2:
		InitializeBackground();
		++loadStep_;
		break;
	case 3:
		ChangeState(std::make_unique<StageSelectSelectingState>());
		state_ = State::Selecting;
		isLoadReady_ = true;
		++loadStep_;
		break;
	default:
		break;
	}
}

bool StageSelectScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

bool StageSelectScene::IsEntranceTransitionFinished() const
{
	return !sceneManager_ || !sceneManager_->IsTransitioning(); // Crackを含むフェード処理が完全に終わってから解放演出を開始する。
}

void StageSelectScene::InitializeStages()
{
	stages_.clear();
	stages_.push_back({
		0u,
		"始まりの平原",
		"WAVE",
		"UI/StageSelect/stage01_preview.dds",
		"基本戦闘を学ぶウェーブ制ステージ",
		"",
		false,
		0u,
		{ 0.18f, 0.49f, 0.20f, 1.0f },
		false
		});
	stages_.push_back({
		1u,
		"忘れられた坑道",
		"SEARCH",
		"UI/StageSelect/stage02_preview.dds",
		"ルート探索と装置起動を進める探索ステージ",
		"Stage 1 クリアで解放",
		true,
		0u,
		{ 0.43f, 0.30f, 0.25f, 1.0f },
		false
		});
	stages_.push_back({
		2u,
		"旧防衛拠点",
		"DEFENSE",
		"UI/StageSelect/stage03_preview.dds",
		"波状攻撃から拠点を守り抜く防衛ステージ",
		"Stage 2 クリアで解放",
		true,
		0u,
		{ 0.25f, 0.38f, 0.62f, 1.0f },
		false
		});
	stages_.push_back({
		3u,
		"崩落都市圏",
		"ESCAPE",
		"UI/StageSelect/stage04_preview.dds",
		"敵をかわしながら出口を目指す脱出ステージ",
		"Stage 3 クリアで解放",
		true,
		0u,
		{ 0.60f, 0.32f, 0.22f, 1.0f },
		false
		});
	stages_.push_back({
		4u,
		"中枢制御塔",
		"BOSS",
		"UI/StageSelect/stage05_preview.dds",
		"最終ボスとの決戦に挑む最終ステージ",
		"Stage 4 クリアで解放",
		true,
		0u,
		{ 0.45f, 0.18f, 0.18f, 1.0f },
		false
		});
}

void StageSelectScene::InitializeSelectors()
{
	if (!dxCommon_) return;
	context_.screenWidth = static_cast<float>(dxCommon_->GetSwapChainDesc().Width);
	context_.screenHeight = static_cast<float>(dxCommon_->GetSwapChainDesc().Height);
	context_.input = input_;
	context_.stages = &stages_;

	context_.onRequestBack = [this]()
	{
		if (pendingUnlockIndex_ >= 0 || nextScene_ != NextScene::None) return;
		nextScene_ = NextScene::Title;
		BackToTitle();
	};

	context_.onRequestMap = [this](uint32_t stageIndex)
	{
		if (pendingUnlockIndex_ >= 0 || nextScene_ != NextScene::None || stages_.empty()) return;
		const int selectedIndex = std::clamp(static_cast<int>(stageIndex), 0, static_cast<int>(stages_.size()) - 1);
		if (stages_[static_cast<size_t>(selectedIndex)].locked) return;
		currentStageIndex_ = selectedIndex;
		auto& repository = StageRepository::GetInstance();
		repository.SetStages(stages_);
		repository.SetStartIndex(selectedIndex); // 決定した中央カードをGamePlay開始ステージとして明示的に確定する。
		nextScene_ = NextScene::GamePlay;
		GoToGamePlay();
	};
}

void StageSelectScene::InitializeBackground()
{
	auto& repository = StageRepository::GetInstance();
	const auto& savedStages = repository.GetStages();
	if (!savedStages.empty() && savedStages.size() == stages_.size())
	{
		stages_ = savedStages;
	}

	int startIndex = 0;
	if (const auto savedIndex = repository.GetStartIndex())
	{
		startIndex = std::clamp(*savedIndex, 0, static_cast<int>(stages_.size()) - 1);
	}

	pendingUnlockIndex_ = -1;
	for (int index = 0; index < static_cast<int>(stages_.size()); ++index)
	{
		if (!stages_[index].justUnlocked) continue;
		pendingUnlockIndex_ = index;
		stages_[index].locked = true;
		startIndex = std::max(0, index - 1);
		// 解除演出が終わるまでロック状態を保存し、途中終了しても次回に演出を再開できるようにする。
		repository.SetStages(stages_);
		repository.SetStartIndex(startIndex);
		break;
	}

	bg_ = std::make_unique<K4E::Sprite>();
	bg_->Initialize("Effects/white.dds");
	bg_->SetPosition({});

	// StageSelectは固定内部解像度で組み、最終Viewportで全体を同じ比率に拡縮する。
	context_.screenWidth = static_cast<float>(K4E::GameViewportConstants::Width);
	context_.screenHeight = static_cast<float>(K4E::GameViewportConstants::Height);
	bg_->SetSize({ context_.screenWidth, context_.screenHeight });
	bg_->SetColor(bgNow_);
	bg_->Update();

	gridSelector_ = std::make_unique<GridStageSelector>();
	gridSelector_->Initialize(context_);
	static_cast<GridStageSelector*>(gridSelector_.get())->SetOnCenterChanged(
		[this](uint32_t centerIndex)
		{
			if (centerIndex >= stages_.size()) return;
			currentStageIndex_ = static_cast<int>(centerIndex);
			bgTarget_ = stages_[centerIndex].color;
			SyncStageSelectUI();
		});

	activeSelector_ = gridSelector_.get();
	currentStageIndex_ = startIndex;
	activeSelector_->FocusToIndex(startIndex, false);

	bgNow_ = bgTarget_ = stages_[startIndex].color;
	bg_->SetColor(bgNow_);
	bg_->Update();

	InitializeStageSelectUI();
	SyncStageSelectUI();
	activeSelector_->OnEnter();
}

void StageSelectScene::InitializeStageSelectUI()
{
	if (stageSelectUIActor_) return;
	auto& uiActor = uiActorWorld_.SpawnActor<StageSelectUIActor>();
	uiActor.SetName("Stage Select UI");
	uiActor.SetLayer("UI");
	uiActor.AddTag("StageSelect");
	uiActor.SetViewportSize(context_.screenWidth, context_.screenHeight);
	stageSelectUIActor_ = &uiActor;
	syncedUiStageIndex_ = -1;
}

void StageSelectScene::SyncStageSelectUI()
{
	if (!stageSelectUIActor_ || stages_.empty()) return;
	const int safeIndex = std::clamp(currentStageIndex_, 0, static_cast<int>(stages_.size()) - 1);
	stageSelectUIActor_->SetViewportSize(context_.screenWidth, context_.screenHeight);
	if (safeIndex != syncedUiStageIndex_)
	{
		stageSelectUIActor_->SetStageInfo(stages_[safeIndex]);
		syncedUiStageIndex_ = safeIndex;
	}
}

void StageSelectScene::RefreshStageSelectUI()
{
	syncedUiStageIndex_ = -1;
	SyncStageSelectUI();
}

void StageSelectScene::ChangeState(std::unique_ptr<IStageSelectSceneState> newState)
{
	if (currentState_)
	{
		currentState_->Exit(this);
	}
	currentState_ = std::move(newState);
	if (currentState_)
	{
		currentState_->Enter(this);
	}
}

void StageSelectScene::BackToTitle()
{
	if (sceneManager_)
	{
		sceneManager_->ChangeScene("TitleScene");
	}
}

void StageSelectScene::GoToGamePlay()
{
	if (sceneManager_ && !stages_.empty())
	{
		const int selectedIndex = std::clamp(currentStageIndex_, 0, static_cast<int>(stages_.size()) - 1);
		auto& repository = StageRepository::GetInstance();
		repository.SetStages(stages_);
		repository.SetStartIndex(selectedIndex); // Scene切替直前にも選択値を再保存し、ロード開始時の0番Fallbackを防ぐ。
		sceneManager_->ChangeScene("GamePlayScene");
	}
}
