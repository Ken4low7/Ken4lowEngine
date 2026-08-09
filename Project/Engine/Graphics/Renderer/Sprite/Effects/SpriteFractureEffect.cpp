#define NOMINMAX
#include "SpriteFractureEffect.h"
#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

	float SpriteFractureEffect::RandRange(float a, float b)
	{
		std::uniform_real_distribution<float> dist(a, b);
		return dist(rng_);
	}

	float SpriteFractureEffect::Clamp01(float v)
	{
		return std::max(0.0f, std::min(1.0f, v));
	}

	float SpriteFractureEffect::Length(float x, float y)
	{
		return std::sqrt(x * x + y * y);
	}

	void SpriteFractureEffect::Normalize(float& x, float& y)
	{
		float len = std::sqrt(x * x + y * y);
		if (len > 1e-6f) { x /= len; y /= len; }
	}

	float SpriteFractureEffect::NormalizedDist01(const Vector2& uv, const Vector2& hitUV)
	{
		// UV空間で中心からの距離を、最大距離(sqrt(0.5^2+0.5^2))で正規化
		const float maxDist = 0.70710678f;
		float dx = uv.x - hitUV.x;
		float dy = uv.y - hitUV.y;
		float d = std::sqrt(dx * dx + dy * dy) / maxDist;
		return Clamp01(d);
	}

	void SpriteFractureEffect::SetProgress(float p)
	{
		progress_ = Clamp01(p);
	}

	void SpriteFractureEffect::Initialize(const Sprite& sourceSprite, int gridX, int gridY)
	{
		Reset();

		gridX_ = std::max(1, gridX);
		gridY_ = std::max(1, gridY);

		filePath_ = sourceSprite.GetFilePath();

		// source の位置（アンカー考慮して左上に直す）
		const Vector2& srcPos = sourceSprite.GetPosition();
		const Vector2& srcSize = sourceSprite.GetSize();
		const Vector2& srcAnchor = sourceSprite.GetAnchorPoint();

		srcSize_ = srcSize;
		srcTopLeft_.x = srcPos.x - srcSize.x * srcAnchor.x;
		srcTopLeft_.y = srcPos.y - srcSize.y * srcAnchor.y;

		srcTexLT_ = sourceSprite.GetTextureLeftTop();
		srcTexSize_ = sourceSprite.GetTextureSize(); // const版がある前提

		pieces_.reserve(static_cast<size_t>(gridX_ * gridY_));

		float pieceW = srcSize_.x / (float)gridX_;
		float pieceH = srcSize_.y / (float)gridY_;

		float uvW = srcTexSize_.x / (float)gridX_;
		float uvH = srcTexSize_.y / (float)gridY_;

		for (int y = 0; y < gridY_; ++y)
		{
			for (int x = 0; x < gridX_; ++x)
			{
				Piece p{};
				p.sprite = std::make_unique<Sprite>();
				p.sprite->Initialize(filePath_);

				// 破片は中心回転
				p.sprite->SetAnchorPoint({ 0.5f, 0.5f });

				// 破片サイズ
				p.sprite->SetSize({ pieceW, pieceH });

				// 破片の中心座標（スクリーン）
				Vector2 center{};
				center.x = srcTopLeft_.x + (x + 0.5f) * pieceW;
				center.y = srcTopLeft_.y + (y + 0.5f) * pieceH;

				p.initialPos = center;
				p.pos = center;
				p.sprite->SetPosition(center);

				// UV切り出し（ピクセル指定）
				Vector2 uvLT{};
				uvLT.x = srcTexLT_.x + x * uvW;
				uvLT.y = srcTexLT_.y + y * uvH;

				Vector2 uvSize{ uvW, uvH };
				p.sprite->SetUVRect(uvLT, uvSize);

				// 破片の UV 中心（0..1）
				p.uvCenter.x = (x + 0.5f) / (float)gridX_;
				p.uvCenter.y = (y + 0.5f) / (float)gridY_;

				// 剥がれ順に少しランダム補正（固定）
				p.bias = RandRange(-0.06f, 0.06f);

				// 初期色
				p.sprite->SetColor({ 1, 1, 1, 1 });

				pieces_.push_back(std::move(p));
			}
		}
	}

	void SpriteFractureEffect::Reset()
	{
		pieces_.clear();
		progress_ = 0.0f;
		finished_ = false;
		hitUV_ = { 0.5f, 0.5f };
	}

	void SpriteFractureEffect::Update(float dt)
	{
		if (pieces_.empty())
		{
			finished_ = true;
			return;
		}

		bool anyAlive = false;

		for (auto& p : pieces_)
		{
			if (p.dead) { continue; }

			// まだ剥がれてない → progress が閾値超えたら剥がす
			if (!p.detached)
			{
				// ★ここが重要：hitUV を毎フレーム反映して閾値を計算
				float th = Clamp01(NormalizedDist01(p.uvCenter, hitUV_) + p.bias);

				if (progress_ >= th)
				{
					p.detached = true;
					p.age = 0.0f;

					// hitUV から外側へ飛び散る方向（UVベース）
					float dx = p.uvCenter.x - hitUV_.x;
					float dy = p.uvCenter.y - hitUV_.y;
					Normalize(dx, dy);

					// 速度：外側 + ちょい上（-Y）に跳ねる
					p.vel.x = dx * impulse_ + RandRange(-220.0f, 220.0f);
					p.vel.y = dy * impulse_ - impulse_ * 0.55f + RandRange(-220.0f, 220.0f);

					// 角速度
					p.angVel = RandRange(-8.0f, 8.0f);
				}
				else
				{
					// くっついてる間は固定
					p.pos = p.initialPos;
					p.sprite->SetPosition(p.pos);
					p.sprite->SetRotation(0.0f);
					p.sprite->SetColor({ 1, 1, 1, 1 });
					p.sprite->Update();

					anyAlive = true;
					continue;
				}
			}

			// 剥がれた後：重力＋積分
			p.age += dt;

			p.vel.y += gravity_ * dt; // +Yが下
			p.pos.x += p.vel.x * dt;
			p.pos.y += p.vel.y * dt;

			p.rot += p.angVel * dt;

			// フェードアウト
			float alpha = 1.0f;
			float fadeStart = std::max(0.0f, lifeTime_ - fadeOut_);
			if (p.age >= fadeStart)
			{
				float t = (p.age - fadeStart) / std::max(0.0001f, fadeOut_);
				alpha = 1.0f - Clamp01(t);
			}

			if (p.age >= lifeTime_)
			{
				p.dead = true;
				continue;
			}

			p.sprite->SetPosition(p.pos);
			p.sprite->SetRotation(p.rot);
			p.sprite->SetColor({ 1, 1, 1, alpha });
			p.sprite->Update();

			anyAlive = true;
		}

		finished_ = !anyAlive;
	}

	void SpriteFractureEffect::Draw()
	{
		for (auto& p : pieces_)
		{
			if (p.dead) { continue; }
			p.sprite->Draw();
		}
	}

} // namespace Ken4lowEngine
