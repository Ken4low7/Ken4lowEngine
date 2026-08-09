#include "SpriteDebrisEmitter.h"
#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

	void SpriteDebrisEmitter::Initialize(const std::string& debrisAtlasPath, const Params& p)
	{
		params_ = p;
		particles_.clear();
		particles_.resize(params_.maxParticles);

		std::random_device rd;
		rng_ = std::mt19937(rd());

		// Spriteを先に作っておく（プール）
		for (auto& pt : particles_)
		{
			pt.sprite = std::make_unique<Sprite>();
			pt.sprite->Initialize(debrisAtlasPath);
			pt.sprite->SetAnchorPoint({ 0.5f, 0.5f });
			pt.sprite->SetColor({ 1, 1, 1, 0 }); // 非表示
			pt.sprite->Update();
			pt.alive = false;
		}
	}

	void SpriteDebrisEmitter::Reset()
	{
		for (auto& pt : particles_)
		{
			pt.alive = false;
			pt.age = 0.0f;
			pt.sprite->SetColor({ 1, 1, 1, 0 });
			pt.sprite->Update();
		}
	}

	int SpriteDebrisEmitter::FindFreeIndex_()
	{
		for (int i = 0; i < (int)particles_.size(); ++i)
		{
			if (!particles_[i].alive) return i;
		}
		return -1;
	}

	float SpriteDebrisEmitter::Rand_(float a, float b)
	{
		std::uniform_real_distribution<float> dist(a, b);
		return dist(rng_);
	}
	int SpriteDebrisEmitter::RandI_(int a, int b)
	{
		std::uniform_int_distribution<int> dist(a, b);
		return dist(rng_);
	}

	void SpriteDebrisEmitter::Burst(const Vector2& center, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			int idx = FindFreeIndex_();
			if (idx < 0) return;

			auto& pt = particles_[idx];
			pt.alive = true;
			pt.age = 0.0f;
			pt.life = Rand_(params_.minLife, params_.maxLife);

			// タイル内でばらける
			pt.pos = {
				center.x + Rand_(-18.0f, 18.0f),
				center.y + Rand_(-18.0f, 18.0f)
			};

			// 上方向にも少し飛ばす（y+が下ならマイナスが上）
			float ang = Rand_(0.0f, 6.2831853f);
			float spd = Rand_(params_.minSpeed, params_.maxSpeed);
			pt.vel = { std::cos(ang) * spd, std::sin(ang) * spd - Rand_(80.0f, 220.0f) };

			pt.rot = Rand_(0.0f, 6.2831853f);
			pt.rotVel = Rand_(-8.0f, 8.0f);

			pt.size = Rand_(params_.minSize, params_.maxSize);

			pt.frame = RandI_(0, params_.atlasFrames - 1);

			// UV（横8枚）
			Vector2 lt = { params_.frameW * (float)pt.frame, 0.0f };
			Vector2 sz = { params_.frameW, params_.frameH };
			pt.sprite->SetUVRect(lt, sz);

			pt.sprite->SetPosition(pt.pos);
			pt.sprite->SetSize({ pt.size, pt.size });
			pt.sprite->SetRotation(pt.rot);

			pt.sprite->SetColor({ 1, 1, 1, 1 });
			pt.sprite->Update();
		}
	}

	void SpriteDebrisEmitter::Update(float dt)
	{
		for (auto& pt : particles_)
		{
			if (!pt.alive) continue;

			pt.age += dt;
			if (pt.age >= pt.life)
			{
				pt.alive = false;
				pt.sprite->SetColor({ 1, 1, 1, 0 });
				pt.sprite->Update();
				continue;
			}

			// 物理
			pt.vel.y += params_.gravity * dt;
			pt.vel.x *= std::pow(params_.damping, dt * 60.0f);
			pt.vel.y *= std::pow(params_.damping, dt * 60.0f);

			pt.pos.x += pt.vel.x * dt;
			pt.pos.y += pt.vel.y * dt;

			// 地面バウンド
			if (pt.pos.y >= params_.groundY)
			{
				pt.pos.y = params_.groundY;
				if (pt.vel.y > 0.0f)
				{
					pt.vel.y = -pt.vel.y * params_.bounce;
					pt.vel.x *= params_.friction;
				}
			}

			// 回転
			pt.rot += pt.rotVel * dt;

			// フェードアウト（最後の30%で消える）
			float t = pt.age / pt.life;
			float a = 1.0f;
			if (t > 0.70f) a = (1.0f - (t - 0.70f) / 0.30f);
			a = std::clamp(a, 0.0f, 1.0f);

			pt.sprite->SetPosition(pt.pos);
			pt.sprite->SetRotation(pt.rot);
			pt.sprite->SetSize({ pt.size, pt.size });
			pt.sprite->SetColor({ 1, 1, 1, a });
			pt.sprite->Update();
		}
	}

	void SpriteDebrisEmitter::Draw()
	{
		for (auto& pt : particles_)
		{
			if (!pt.alive) continue;
			pt.sprite->Draw();
		}
	}

} // namespace Ken4lowEngine
