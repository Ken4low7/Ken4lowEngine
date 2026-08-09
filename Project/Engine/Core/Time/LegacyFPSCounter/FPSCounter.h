#pragma once
#include <chrono>
#include <thread>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///						FPSカウンタクラス
	/// -------------------------------------------------------------
	class FPSCounter
	{
	private: /// ---------- 型エイリアス ---------- ///
		
		// steady_clockを使い、システム時刻変更の影響を受けにくい計測を行う。
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		/// <summary>
		/// chronoのdurationをミリ秒のfloat値に変換します。
		/// </summary>
		/// <param name="d">変換する時間幅。</param>
		/// <returns>ミリ秒単位の時間。</returns>
		float ToMs(const Clock::duration& d) const
		{
			return std::chrono::duration<float, std::milli>(d).count();
		}

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// FPSCounterを生成し、目標FPSと基準時刻を初期化します。
		/// </summary>
		/// <param name="targetFPS">目標FPS。</param>
		FPSCounter(int targetFPS = 144);

		/// <summary>
		/// フレーム開始時に呼び出し、デルタタイム計算と区間計測値のリセットを行います。
		/// </summary>
		void StartFrame();

		/// <summary>
		/// Update処理の開始時刻を記録します。
		/// </summary>
		void BeginUpdate();

		/// <summary>
		/// Update処理にかかった時間をミリ秒単位で記録します。
		/// </summary>
		void EndUpdate();

		/// <summary>
		/// Draw処理の開始時刻を記録します。
		/// </summary>
		void BeginDraw();

		/// <summary>
		/// Draw処理にかかった時間をミリ秒単位で記録します。
		/// </summary>
		void EndDraw();

		/// <summary>
		/// Present処理の開始時刻を記録します。
		/// </summary>
		void BeginPresent();

		/// <summary>
		/// Present処理にかかった時間をミリ秒単位で記録します。
		/// </summary>
		void EndPresent();

		/// <summary>
		/// フレーム終了時に呼び出し、総フレーム時間とFPSを更新します。
		/// </summary>
		void EndFrame();

		/// <summary>
		/// FPSCounterの計測値と基準時刻を初期状態に戻します。
		/// </summary>
		/// <param name="targetFPS">再設定する目標FPS。</param>
		void Reset(int targetFPS = 144);

	public: /// ---------- Getter ---------- ///

		/// <summary>
		/// 現在計測されているFPSを取得します。
		/// </summary>
		float GetFPS() const { return currentFPS_; }

		/// <summary>
		/// 前フレームから現在フレームまでの経過時間を秒単位で取得します。
		/// </summary>
		float GetDeltaTime() const { return deltaSecond_; }

		/// <summary>
		/// Update処理時間をミリ秒単位で取得します。
		/// </summary>
		float GetUpdateMs() const { return updateMs_; }

		/// <summary>
		/// Draw処理時間をミリ秒単位で取得します。
		/// </summary>
		float GetDrawMs() const { return drawMs_; }

		/// <summary>
		/// Present処理時間をミリ秒単位で取得します。
		/// </summary>
		float GetPresentMs() const { return presentMs_; }

		/// <summary>
		/// FPS固定処理で待機した時間をミリ秒単位で取得します。
		/// </summary>
		float GetSleepMs() const { return sleepMs_; }

		/// <summary>
		/// 1フレーム全体の処理時間をミリ秒単位で取得します。
		/// </summary>
		float GetTotalFrameMs() const { return totalFrameMs_; }

	public: /// ---------- Setter ---------- ///

		/// <summary>
		/// 目標FPSを設定します。
		/// </summary>
		/// <param name="fps">新しく設定する目標FPS。</param>
		void SetTargetFPS(int fps) { targetFPS_ = fps; }

	private: /// ---------- メンバ変数 ---------- ///

		// 1フレーム全体の開始時刻
		TimePoint frameBegin_{};

		// 前回フレーム開始時刻。デルタタイム計算に使用する
		TimePoint lastBegin_{};
		
		// FPS計算の基準時刻。約1秒ごとに更新する
		TimePoint fpsReference_{};

		// Update/Draw/Presentそれぞれの開始時刻
		TimePoint updateBegin_{};
		TimePoint drawBegin_{};
		TimePoint presentBegin_{};

		// 前フレームから現在フレームまでの経過時間（秒）
		float deltaSecond_ = 0.0f;

		// 各区間の処理時間（ミリ秒）
		float updateMs_ = 0.0f;
		float drawMs_ = 0.0f;
		float presentMs_ = 0.0f;
		float sleepMs_ = 0.0f;
		float totalFrameMs_ = 0.0f;

		// FPS計算と目標FPS管理に使用する値
		int targetFPS_ = 144;
		int frameCount_ = 0;
		float currentFPS_ = 0.0f;
	};
}
