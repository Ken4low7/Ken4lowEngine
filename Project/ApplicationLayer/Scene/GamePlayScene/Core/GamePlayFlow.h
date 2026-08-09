#pragma once
#include "PauseMenu.h"
#include "ResultMenu.h"

#include <functional>
#include <memory>

namespace Ken4lowEngine { class Input; class SceneManager; }

class HUDManager;
class IPlayerRuntime;

class GamePlayFlow
{
public:
	enum class State
	{
		Intro,
		EquipIntro,
		Playing,
		GameClear,
		GameOver,
	};

	struct PausedUpdateContext
	{
		float deltaTime;
		Ken4lowEngine::Input* input;
		HUDManager* hud = nullptr;
		IPlayerRuntime* player = nullptr;
		Ken4lowEngine::SceneManager* sceneManager = nullptr;
		bool lockCursorOnResume = true;
	};

	struct ResultUpdateContext
	{
		float deltaTime;
		Ken4lowEngine::Input* input;
		Ken4lowEngine::SceneManager* sceneManager = nullptr;
		std::function<void()> onRetry;
		std::function<void()> onNextStage;
	};

	void Initialize();
	void Finalize();
	void ResetForNewGame(bool startIntro);
	void StartPlaying();
	void EnterPause(Ken4lowEngine::Input* input);
	void ExitPause(Ken4lowEngine::Input* input, bool lockCursorOnResume);
	void CancelPause();
	void EnterGameClear(Ken4lowEngine::Input* input, const std::function<void()>& onUnlockNextStage);
	void EnterGameOver(Ken4lowEngine::Input* input);
	void UpdatePaused(const PausedUpdateContext& ctx);
	void UpdateResult(const ResultUpdateContext& ctx);
	void DrawUI();

	bool IsPaused() const { return isPaused_; }
	bool IsIntro() const { return state_ == State::Intro; }
	bool IsEquipIntro() const { return state_ == State::EquipIntro; }
	bool IsResultState() const { return state_ == State::GameClear || state_ == State::GameOver; }
	State GetState() const { return state_; }
	void SetState(State state) { state_ = state; }

private:
	std::unique_ptr<PauseMenu> pauseMenu_;
	std::unique_ptr<ResultMenu> resultMenu_;
	bool isPaused_ = false;
	float resultInputCooldown_ = 0.0f;
	State state_ = State::Playing;
};
