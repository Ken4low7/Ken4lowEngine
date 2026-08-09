#pragma once
#include "FPSCounter.h"

namespace Ken4lowEngine
{
	/// --------------------------------------------------------------
	///						GameTimerクラス
	/// ---------------------------------------------------------------
	class GameTimer
	{
	public:
		struct CompletedFrameTiming
		{
			float frameIntervalMs = 0.0f;
			float updateMs = 0.0f;
			float drawMs = 0.0f;
			float presentMs = 0.0f;
			float totalFrameMs = 0.0f;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// GameTimerのシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>GameTimerの唯一のインスタンス。</returns>
		static GameTimer* GetInstance();

		/// <summary>
		/// タイマーを初期化し、目標FPSを設定します。
		/// </summary>
		/// <param name="targetFPS">目標FPS。</param>
		void Initialize(int targetFPS = 144);

		/// <summary>
		/// タイマーの使用状態を終了状態にします。
		/// </summary>
		void Finalize();

		/// <summary>
		/// 1フレームの開始時に呼び出し、デルタタイムと各区間計測値を更新準備します。
		/// </summary>
		void BeginFrame();

		/// <summary>
		/// 1フレームの終了時に呼び出し、FPS計算とデバッグログ出力を行います。
		/// </summary>
		void EndFrame();

		/// <summary>
		/// Update処理区間の計測を開始します。
		/// </summary>
		void BeginUpdate();

		/// <summary>
		/// Update処理区間の計測を終了します。
		/// </summary>
		void EndUpdate();

		/// <summary>
		/// Draw処理区間の計測を開始します。
		/// </summary>
		void BeginDraw();

		/// <summary>
		/// Draw処理区間の計測を終了します。
		/// </summary>
		void EndDraw();

		/// <summary>
		/// Present処理区間の計測を開始します。
		/// </summary>
		void BeginPresent();

		/// <summary>
		/// Present処理区間の計測を終了します。
		/// </summary>
		void EndPresent();

	public: /// ---------- Getter ---------- ///

		/// <summary>
		/// 前フレームから現在フレームまでの経過時間を秒単位で取得します。
		/// </summary>
		float GetDeltaTime() const { return fpsCounter_.GetDeltaTime(); }

		/// <summary>
		/// 現在計測されているFPSを取得します。
		/// </summary>
		float GetFPS() const { return fpsCounter_.GetFPS(); }

		/// <summary>
		/// 現在設定されている目標FPSを取得します。
		/// </summary>
		int GetTargetFPS() const { return targetFPS_; }

		/// <summary>
		/// 直近のUpdate処理にかかった時間をミリ秒単位で取得します。
		/// </summary>
		float GetUpdateMs() const { return fpsCounter_.GetUpdateMs(); }

		/// <summary>
		/// 直近のDraw処理にかかった時間をミリ秒単位で取得します。
		/// </summary>
		float GetDrawMs() const { return fpsCounter_.GetDrawMs(); }

		/// <summary>
		/// 直近のPresent処理にかかった時間をミリ秒単位で取得します。
		/// </summary>
		float GetPresentMs() const { return fpsCounter_.GetPresentMs(); }

		/// <summary>
		/// FPS固定処理で待機した時間をミリ秒単位で取得します。
		/// </summary>
		float GetSleepMs() const { return fpsCounter_.GetSleepMs(); }

		/// <summary>
		/// 直近1フレーム全体にかかった時間をミリ秒単位で取得します。
		/// </summary>
		float GetTotalFrameMs() const { return fpsCounter_.GetTotalFrameMs(); }

		/// 完了済みフレームの値を返し、次フレーム開始時のFPSCounterリセット後も参照できるようにする。
		const CompletedFrameTiming& GetCompletedFrameTiming() const { return completedFrameTiming_; }

	public: /// ---------- Setter ---------- ///

		/// <summary>
		/// 目標FPSを変更し、内部のFPSCounterにも反映します。
		/// </summary>
		/// <param name="fps">新しく設定する目標FPS。</param>
		void SetTargetFPS(int fps)
		{
			// GameTimer側とFPSCounter側の目標FPSを一致させる。
			targetFPS_ = fps;
			fpsCounter_.SetTargetFPS(fps);
		}

	private: /// ---------- メンバ変数 ---------- ///

		// FPSや各処理区間の計測を担当する実体。
		FPSCounter fpsCounter_{ 144 };

		// 現在設定されている目標FPS。
		int targetFPS_ = 144;

		// Initialize済みかどうかを管理し、未初期化状態での計測呼び出しを防ぐ。
		bool initialized_ = false;

		// Update区間が開いたままEndFrameされた旧呼び出し経路を安全にEndUpdateへ補正する。
		bool updatePhaseActive_ = false;

		// 前回最後まで完了したフレームの計測値をEditor診断用に保持する。
		CompletedFrameTiming completedFrameTiming_{};

	private: /// ---------- コピー禁止 ---------- ///

		// シングルトンとして扱うため、外部からの生成・破棄・コピーを禁止する。
		GameTimer() = default;
		~GameTimer() = default;
		GameTimer(const GameTimer&) = delete;
		GameTimer& operator=(const GameTimer&) = delete;

	};
} // namespace Ken4lowEngine
