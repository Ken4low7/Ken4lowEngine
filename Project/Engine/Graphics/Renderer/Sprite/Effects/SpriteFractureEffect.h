#pragma once
#include "Sprite.h"
#include <memory>
#include <vector>
#include <random>

namespace Ken4lowEngine
{

	class SpriteFractureEffect
	{
	public:
		// sourceSprite を gridX * gridY に分割して破片を準備
		void Initialize(const Sprite& sourceSprite, int gridX, int gridY);

		// ひび中心（UV 0..1）と進行度（0..1）
		void SetHitUV(const Vector2& hitUV) { hitUV_ = hitUV; }
		void SetProgress(float p);

		// 物理パラメータ
		void SetGravity(float g) { gravity_ = g; }        // +Y が下方向（Sprite座標）
		void SetImpulse(float v) { impulse_ = v; }        // 剥がれる瞬間の飛び散り
		void SetLifetime(float sec) { lifeTime_ = sec; }  // 破片の生存時間
		void SetFadeOut(float sec) { fadeOut_ = sec; }    // 消える時間

		// 毎フレーム
		void Update(float dt);
		void Draw();

		// 全部消えた？
		bool IsFinished() const { return finished_; }

		// もう一度使う時
		void Reset();

	private:
		struct Piece
		{
			std::unique_ptr<Sprite> sprite;

			Vector2 uvCenter{ 0.5f, 0.5f }; // 破片の“UV上の中心”
			float bias = 0.0f;              // 剥がれ順のランダム補正

			Vector2 initialPos;   // 初期位置（中心）
			Vector2 pos;          // 現在位置（中心）
			Vector2 vel;          // 速度

			float rot = 0.0f;
			float angVel = 0.0f;

			bool detached = false; // 剥がれたか
			float age = 0.0f;      // 剥がれてからの時間
			bool dead = false;
		};

	private:
		// source 情報
		std::string filePath_;
		Vector2 srcTopLeft_{};
		Vector2 srcSize_{};
		Vector2 srcTexLT_{};
		Vector2 srcTexSize_{};

		int gridX_ = 1;
		int gridY_ = 1;

		// 制御
		Vector2 hitUV_{ 0.5f, 0.5f };
		float progress_ = 0.0f;
		bool finished_ = false;

		// 物理
		float gravity_ = 2200.0f;  // 強め（見栄えが出る）
		float impulse_ = 800.0f;
		float lifeTime_ = 1.6f;
		float fadeOut_ = 0.5f;

		std::vector<Piece> pieces_;
		std::mt19937 rng_{ 12345 };

	private:
		float RandRange(float a, float b);
		static float Clamp01(float v);
		static float Length(float x, float y);
		static void Normalize(float& x, float& y);

		// hitUV からの距離を 0..1 に正規化（中心0、四隅1）
		static float NormalizedDist01(const Vector2& uv, const Vector2& hitUV);
	};

} // namespace Ken4lowEngine
