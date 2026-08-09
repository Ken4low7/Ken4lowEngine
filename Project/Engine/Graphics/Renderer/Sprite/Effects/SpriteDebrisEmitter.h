#pragma once
#include <vector>
#include <memory>
#include <random>
#include "Sprite.h"

namespace Ken4lowEngine
{

	class SpriteDebrisEmitter
	{
	public:
		struct Params
		{
			int   maxParticles = 256;

			// 物理
			float gravity = 2200.0f;       // y+が下方向想定
			float damping = 0.985f;        // 空気抵抗（速度減衰）
			float bounce = 0.25f;          // 地面で反射（0..1）
			float friction = 0.80f;        // 接地時のx減衰

			// 見た目
			float minLife = 0.25f;
			float maxLife = 0.75f;

			float minSize = 6.0f;
			float maxSize = 14.0f;

			float minSpeed = 120.0f;
			float maxSpeed = 520.0f;

			// DebrisAtlas（横8枚×縦1枚、1コマ32×32）
			int   atlasFrames = 8;
			float frameW = 32.0f;
			float frameH = 32.0f;

			// 地面y（落ちたら跳ね返る）
			float groundY = 600.0f;
		};

		void Initialize(const std::string& debrisAtlasPath, const Params& p);
		void Reset();

		// ひび割れの位置（例：タイルの中心）付近にバースト
		void Burst(const Vector2& center, int count);

		void Update(float dt);
		void Draw();

		// ImGui用に取り出し
		Params& GetParams() { return params_; }

	private: /// ---------- 構造体 ---------- ///

		struct Particle
		{
			bool alive = false;

			Vector2 pos{};
			Vector2 vel{};
			float rot = 0.0f;
			float rotVel = 0.0f;

			float life = 1.0f;
			float age = 0.0f;

			float size = 8.0f;
			int frame = 0;

			std::unique_ptr<Sprite> sprite;
		};

		Params params_{};
		std::vector<Particle> particles_;
		std::mt19937 rng_{ 1234 };

		int FindFreeIndex_();
		float Rand_(float a, float b);
		int RandI_(int a, int b);
	};

} // namespace Ken4lowEngine
