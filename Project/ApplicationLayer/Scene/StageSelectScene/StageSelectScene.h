#pragma once

#include "BaseScene.h"
#include "GridStageSelector.h"
#include "IStageSelector.h"
#include "IStageSelectSceneState.h"

#include <ActorWorld.h>
#include <Sprite.h>

#include <memory>
#include <vector>

namespace K4E = ::Ken4lowEngine;

class StageSelectUIActor;

namespace Ken4lowEngine
{
	class DirectXCommon;
	class Input;
}

/// -------------------------------------------------------------
/// ステージセレクトシーン。
/// サムネイル操作はIStageSelector、文字UIはStageSelectUIActorへ分離する。
/// -------------------------------------------------------------
class StageSelectScene : public K4E::BaseScene
{
public:
	enum class State
	{
		Selecting,
		Loading,
	};

	enum class NextScene
	{
		None,
		Title,
		GamePlay,
	};

public:
	~StageSelectScene() override = default;

	void Initialize() override;
	void Update() override;
	void UpdateEditor(float deltaTime) override;
	void Draw3DObjects() override;
	void DrawShadowObjects() override;
	void Draw2DSprites() override;
	void Finalize() override;
	void DrawImGui() override;
	void CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects) override;
	K4E::ActorWorld* GetEditorActorWorld() override { return &uiActorWorld_; }

	void StartLoad() override;
	void UpdateLoad() override;
	bool IsReadyToStartUncover() const override;

	void ChangeState(std::unique_ptr<IStageSelectSceneState> newState);

	State GetState() const { return state_; }
	void SetState(State state) { state_ = state; }

	K4E::DirectXCommon* GetDxCommon() const { return dxCommon_; }
	K4E::Input* GetInput() const { return input_; }

	std::vector<StageInfo>& GetStages() { return stages_; }
	const std::vector<StageInfo>& GetStages() const { return stages_; }
	IStageSelector* GetActiveSelector() const { return activeSelector_; }

	K4E::Sprite* GetBgSprite() const { return bg_.get(); }
	K4E::Vector4& GetBgNow() { return bgNow_; }
	K4E::Vector4& GetBgTarget() { return bgTarget_; }

	int& GetPendingUnlockIndex() { return pendingUnlockIndex_; }
	void SetNextScene(NextScene nextScene) { nextScene_ = nextScene; }
	NextScene GetNextScene() const { return nextScene_; }
	SelectorContext& GetSelectorContext() { return context_; }
	int GetCurrentStageIndex() const { return currentStageIndex_; }
	void SetCurrentStageIndex(int index) { currentStageIndex_ = index; }
	void RefreshStageSelectUI();
	bool IsEntranceTransitionFinished() const;

	void BackToTitle();
	void GoToGamePlay();

private:
	void InitializeStages();
	void InitializeSelectors();
	void InitializeBackground();
	void InitializeStageSelectUI();
	void SyncStageSelectUI();

private:
	State state_ = State::Selecting;
	std::unique_ptr<IStageSelectSceneState> currentState_;
	NextScene nextScene_ = NextScene::None;

	K4E::DirectXCommon* dxCommon_ = nullptr;
	K4E::Input* input_ = nullptr;

	std::vector<StageInfo> stages_;
	SelectorContext context_{};
	std::unique_ptr<IStageSelector> gridSelector_;
	IStageSelector* activeSelector_ = nullptr;

	K4E::ActorWorld uiActorWorld_{};
	StageSelectUIActor* stageSelectUIActor_ = nullptr;
	int syncedUiStageIndex_ = -1;

	std::unique_ptr<K4E::Sprite> bg_;
	K4E::Vector4 bgNow_{ 0.18f, 0.49f, 0.20f, 1.0f };
	K4E::Vector4 bgTarget_ = bgNow_;

	int pendingUnlockIndex_ = -1;
	int loadStep_ = 0;
	bool isLoadReady_ = false;
	int currentStageIndex_ = 0;
};