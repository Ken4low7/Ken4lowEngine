#include "TitleLoadState.h"
#include "TitleScene.h"
#include "SceneManager.h"

using namespace Ken4lowEngine;

void TitleLoadState::Enter(TitleScene* scene)
{
	// 安全確認
	if (!scene) return;

	// 念のため状態を初期化しておく
	scene->SetState(TitleScene::State::Loading);

	// タイマーの初期化
	timer_ = 0.0f;
}

void TitleLoadState::Update(TitleScene* scene, float deltaTime)
{
	timer_ += deltaTime;

	// ロード完了＆演出時間経過でステージセレクトへ
	if (timer_ >= duration_)
	{
		if (auto* mgr = scene->GetSceneManager())
		{
			mgr->ChangeScene("StageSelectScene");
		}
	}
}

void TitleLoadState::Exit(TitleScene* scene)
{
	(void)scene; // 未使用引数対策
}
