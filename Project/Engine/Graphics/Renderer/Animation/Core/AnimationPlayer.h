#pragma once

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				  アニメーションプレイヤークラス
	/// -------------------------------------------------------------
	class AnimationPlayer
	{
		/// アニメーション再生状態（プレイヤー）
		/// - 再生/停止
		/// - 時刻の進行
		/// - ループ/速度

	public: /// ---------- メンバ関数 ---------- ///

		// アニメーションのリセット
		void Reset();

		// アニメーションの更新
		void Update(float deltaTime, float animationDuration);

	public: /// ---------- アクセッサ ---------- ///

		// 再生時間の取得・設定
		void SetPlaying(bool value) { isPlaying_ = value; }
		bool IsPlaying() const { return isPlaying_; }

		// 再生速度の取得・設定
		void SetLoop(bool value) { loop_ = value; }
		bool IsLoop() const { return loop_; }

		// 再生速度の取得・設定
		void SetSpeed(float value) { speed_ = value; }
		float GetSpeed() const { return speed_; }

		// 最後まで再生したら停止するかどうか
		void SetStopAtEnd(bool value) { stopAtEnd_ = value; }
		bool IsStopAtEnd() const { return stopAtEnd_; }

		// 再生時間の取得・設定
		void SetTime(float time) { time_ = time; }
		float GetTime() const { return time_; }

		// 前フレームからの経過時間の取得
		float GetDeltaTime() const { return deltaTime_; }

	private: /// ---------- メンバ変数 ---------- ///

		float time_ = 0.0f; // 現在の再生時間（秒）
		float deltaTime_ = 0.0f; // 前フレームからの経過時間（秒）
		bool isPlaying_ = true; // 再生中かどうか

		bool loop_ = true; // ループ再生するかどうか
		float speed_ = 1.0f; // 再生速度（1.0が等倍速）

		bool stopAtEnd_ = true; // 最後まで再生したら停止するかどうか
	};

}