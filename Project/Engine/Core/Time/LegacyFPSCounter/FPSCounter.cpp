#define NOMINMAX
#include "FPSCounter.h"
#include <sstream>
#include <windows.h>
#include <algorithm>

namespace Ken4lowEngine
{
	FPSCounter::FPSCounter(int targetFPS)
		: targetFPS_(targetFPS)
	{
		// 生成直後のフレーム計測基準を現在時刻にそろえる。
		auto now = Clock::now();
		frameBegin_ = now;
		lastBegin_ = now;
		fpsReference_ = now;
	}

	void FPSCounter::StartFrame()
	{
		const auto now = Clock::now();

		// 前回フレーム開始時刻との差分からデルタタイムを計算する。
		if (lastBegin_.time_since_epoch().count() != 0)
		{
			deltaSecond_ = std::chrono::duration<float>(now - lastBegin_).count();
		}
		else
		{
			// 初回など基準時刻がない場合は、目標FPSから仮のデルタタイムを作る。
			deltaSecond_ = 1.0f / static_cast<float>(std::max(1, targetFPS_));
		}

		// 次フレームのデルタタイム計算と、今回フレームの総時間計測用に時刻を保存する。
		lastBegin_ = now;
		frameBegin_ = now;

		// 前フレームの区間計測値が残らないように、フレーム開始時にリセットする。
		updateMs_ = 0.0f;
		drawMs_ = 0.0f;
		presentMs_ = 0.0f;
		sleepMs_ = 0.0f;
		totalFrameMs_ = 0.0f;
	}

	void FPSCounter::BeginUpdate()
	{
		// Update処理の開始時刻を保存する。
		updateBegin_ = Clock::now();
	}

	void FPSCounter::EndUpdate()
	{
		// 現在時刻との差分からUpdate処理時間を算出する。
		updateMs_ = ToMs(Clock::now() - updateBegin_);
	}

	void FPSCounter::BeginDraw()
	{
		// Draw処理の開始時刻を保存する。
		drawBegin_ = Clock::now();
	}

	void FPSCounter::EndDraw()
	{
		// 現在時刻との差分からDraw処理時間を算出する。
		drawMs_ = ToMs(Clock::now() - drawBegin_);
	}

	void FPSCounter::BeginPresent()
	{
		// Present処理の開始時刻を保存する。
		presentBegin_ = Clock::now();
	}

	void FPSCounter::EndPresent()
	{
		// 現在時刻との差分からPresent処理時間を算出する。
		presentMs_ = ToMs(Clock::now() - presentBegin_);
	}

	void FPSCounter::EndFrame()
	{
		const auto frameEnd = Clock::now();

		// フレーム開始から終了までの合計時間を記録する。
		totalFrameMs_ = ToMs(frameEnd - frameBegin_);

		++frameCount_;
		const float elapsedSec = std::chrono::duration<float>(frameEnd - fpsReference_).count();

		// 現在はFPS固定のSleep処理を使っていないため、待機時間は0として扱う。
		sleepMs_ = 0.0f;

		// 約1秒ごとにFPSを更新し、デバッグ出力に区間ごとの処理時間を表示する。
		if (elapsedSec >= 1.0f)
		{
			currentFPS_ = static_cast<float>(frameCount_) / elapsedSec;
			frameCount_ = 0;
			fpsReference_ = frameEnd;

			std::ostringstream oss;
			oss
				<< "FPS: " << currentFPS_
				<< " / Target: " << targetFPS_
				<< " | Update: " << updateMs_ << " ms"
				<< " | Draw: " << drawMs_ << " ms"
				<< " | Present: " << presentMs_ << " ms"
				<< " | Sleep: " << sleepMs_ << " ms"
				<< " | Total: " << totalFrameMs_ << " ms\n";

			OutputDebugStringA(oss.str().c_str());
		}
	}

	void FPSCounter::Reset(int targetFPS)
	{
		// 目標FPSとFPS計測用のカウンターを初期値に戻す。
		targetFPS_ = targetFPS;
		frameCount_ = 0;
		currentFPS_ = 0.0f;
		deltaSecond_ = 0.0f;

		// 各処理区間の計測値もリセットする。
		updateMs_ = 0.0f;
		drawMs_ = 0.0f;
		presentMs_ = 0.0f;
		sleepMs_ = 0.0f;
		totalFrameMs_ = 0.0f;

		// リセット直後から正しいデルタタイムを計測できるように基準時刻をそろえる。
		const auto now = Clock::now();
		frameBegin_ = now;
		lastBegin_ = now;
		fpsReference_ = now;

		// 区間計測の開始時刻は、次回Begin系関数で設定されるため空にしておく。
		updateBegin_ = {};
		drawBegin_ = {};
		presentBegin_ = {};
	}
}
