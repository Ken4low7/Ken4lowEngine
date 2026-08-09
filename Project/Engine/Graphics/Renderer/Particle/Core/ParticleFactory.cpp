#include "ParticleFactory.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　		パーティクル生成
/// -------------------------------------------------------------
Particle ParticleFactory::Create(std::mt19937& randomEngine, const Vector3& position, ParticleEffectType effectType)
{
	Particle particle;

	// エフェクト種別ごとにパーティクルの初期設定を行う
	switch (effectType)
	{
	case ParticleEffectType::Default: // デフォルト
	{
		std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
		std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

		Vector3 randomTranslate{ distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		particle.transform.translate_ = position + randomTranslate;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = distTime(randomEngine);
		particle.velocity = { distribution(randomEngine), distribution(randomEngine), distribution(randomEngine) };
		break;
	}

	case ParticleEffectType::Slash: // スラッシュ
	{
		std::uniform_real_distribution<float> distScale(0.8f, 3.0f);
		std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

		particle.transform.scale_ = { 0.1f, distScale(randomEngine) * 2.0f, 2.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, distRotate(randomEngine) };
		particle.transform.translate_ = position;
		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		particle.lifeTime = 1.0f;
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		break;
	}
	case ParticleEffectType::Ring: // リング
	{
		std::uniform_real_distribution<float> distScale(0.5f, 1.0f);
		std::uniform_real_distribution<float> distTime(0.3f, 0.5f);

		particle.transform.translate_ = position;
		float start = distScale(randomEngine);
		float end = start * 2.5f;

		particle.startScale = { start, start, start };
		particle.endScale = { end, end, end };

		particle.transform.scale_ = particle.startScale;
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		particle.lifeTime = distTime(randomEngine);
		particle.velocity = { 0.0f, 0.0f, 0.0f }; // 拡大で動きを表現する
		break;
	}
	case ParticleEffectType::Blast: // 爆風
	{
		std::uniform_real_distribution<float> distAngle(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
		std::uniform_real_distribution<float> distScale(10.0f, 24.0f);
		std::uniform_real_distribution<float> distLifetime(0.5f, 1.0f);

		particle.transform.translate_ = position;
		particle.transform.scale_ = { 0.1f, 0.1f, 0.1f }; // 初期は小さく

		// ランダムな傾き（回転）
		particle.transform.rotate_ = {
			distAngle(randomEngine),
			distAngle(randomEngine),
			distAngle(randomEngine)
		};

		particle.startScale = particle.transform.scale_;
		particle.endScale = { distScale(randomEngine), distScale(randomEngine), distScale(randomEngine) };

		particle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		particle.lifeTime = distLifetime(randomEngine);
		particle.velocity = {}; // 移動しない（広がるだけ）

		break;
	}
	case ParticleEffectType::Cylinder: // シリンダー
	{
		std::uniform_real_distribution<float> distColor(0.0f, 1.0f);

		particle.transform.translate_ = position;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f }; // 高さ方向にスケール
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = 999.0f; // 一時的にずっと表示

		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		break;
	}
	case ParticleEffectType::Star: // 星型
	{
		std::uniform_real_distribution<float> distColor(0.8f, 1.0f);
		particle.transform.translate_ = position;
		particle.transform.scale_ = { 0.5f, 0.5f, 0.5f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };
		particle.color = { distColor(randomEngine), distColor(randomEngine), distColor(randomEngine), 1.0f };
		particle.lifeTime = 0.3f;
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		break;
	}
	case ParticleEffectType::Smoke: // 煙
	{
		std::uniform_real_distribution<float> distOffset(-0.3f, 0.3f);
		std::uniform_real_distribution<float> distScale(3.0f, 6.0f);
		std::uniform_real_distribution<float> distTime(0.6f, 1.2f);
		std::uniform_real_distribution<float> distGray(0.3f, 0.6f);
		std::uniform_real_distribution<float> distRot(0.0f, std::numbers::pi_v<float> *2.0f);
		std::uniform_real_distribution<float> distDir(-0.1f, 0.1f);

		Vector3 offset = {
			distOffset(randomEngine),
			0.0f,
			distOffset(randomEngine)
		};

		float gray = distGray(randomEngine);
		particle.transform.translate_ = position + offset;
		particle.transform.rotate_.z = distRot(randomEngine);
		particle.color = { gray, gray, gray, 0.2f };
		particle.lifeTime = distTime(randomEngine);

		float scale = distScale(randomEngine);
		particle.startScale = { scale * 0.3f, scale * 0.3f, scale * 0.3f };
		particle.endScale = { scale, scale, scale };

		particle.velocity = { distDir(randomEngine), 0.3f, distDir(randomEngine) };
		break;
	}

	case ParticleEffectType::Flash: // フラッシュ
	{
		std::uniform_real_distribution<float> distColor(0.6f, 1.0f);
		// 一瞬だけ光るフラッシュ
		particle.transform.translate_ = position;
		particle.transform.scale_ = { 3.0f, 3.0f, 3.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 7.0f, 7.0f, 7.0f }; // 画面を覆うサイズに

		particle.color = {
			distColor(randomEngine),         // R（高め）
			distColor(randomEngine) * 0.5f,  // G（控えめ）
			0.0f,                            // Bなし
			1.0f
		};
		particle.lifeTime = 0.04f; // パッと消える
		particle.velocity = { 0.0f, 0.0f, 0.0f }; // 移動なし
		break;
	}

	case ParticleEffectType::Spark: // 火花
	{
		std::uniform_real_distribution<float> distDir(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distSpeed(3.0f, 5.0f);

		Vector3 dir = {
			distDir(randomEngine),
			distDir(randomEngine),
			distDir(randomEngine)
		};
		float speed = distSpeed(randomEngine);

		particle.transform.translate_ = position;
		particle.transform.scale_ = { 0.08f, 0.08f, 0.08f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.02f, 0.02f, 0.02f }; // 完全には消えない

		particle.color = { 1.0f, 0.9f, 0.5f, 1.0f }; // 黄色〜オレンジ
		particle.lifeTime = 0.15f;
		particle.velocity = dir * speed; // 高速で飛ばす
		break;
	}

	case ParticleEffectType::EnergyGather: // 収束
	{
		std::uniform_real_distribution<float> distPos(-3.0f, 3.0f);
		std::uniform_real_distribution<float> distTime(0.4f, 0.8f);
		std::uniform_real_distribution<float> distAlpha(0.5f, 1.0f);

		Vector3 startPos = {
			position.x + distPos(randomEngine),
			position.y + distPos(randomEngine),
			position.z + distPos(randomEngine),
		};

		particle.transform.translate_ = startPos;
		particle.transform.scale_ = { 0.1f, 0.1f, 0.1f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_; // スケールは固定

		particle.color = { 0.5f, 1.0f, 1.0f, distAlpha(randomEngine) }; // 青白光
		particle.lifeTime = distTime(randomEngine);

		// 中心に向かう速度
		Vector3 dir = position - startPos;
		Vector3::Normalize(dir);
		particle.velocity = dir * 3.0f;

		break;
	}
	case ParticleEffectType::Charge: // 収束中にぐるぐる回る
	{
		std::uniform_real_distribution<float> distT(0.0f, std::numbers::pi_v<float> *2.0f);
		std::uniform_real_distribution<float> distRadius(2.0f, 4.0f); // 軌道半径
		std::uniform_real_distribution<float> distLifetime(2.0f, 3.0f);
		std::uniform_real_distribution<float> distColor(0.6f, 1.0f);
		std::uniform_real_distribution<float> distAngle(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

		float t = distT(randomEngine);
		float radius = distRadius(randomEngine);

		// ランダムな回転軸（斜め方向）
		Vector3 axis = {
			std::sin(distAngle(randomEngine)),
			std::cos(distAngle(randomEngine)),
			std::sin(distAngle(randomEngine))
		};
		axis = Vector3::Normalize(axis);

		// 八の字軌道のローカル位置（XZベース）
		Vector3 basePos = {
			radius * std::sin(t),
			0.0f,
			(radius / 1.5f) * std::sin(2.0f * t)
		};

		// 軸にそって初期位置を回転
		Matrix4x4 rotMat = Matrix4x4::MakeRotateAxisAngleMatrix(axis, distAngle(randomEngine));
		Vector3 startPos = Vector3::Transform(basePos, rotMat);

		// パーティクル設定
		particle.transform.translate_ = position + startPos;
		particle.transform.scale_ = { 0.1f, 0.1f, 0.1f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = particle.transform.scale_;
		particle.color = {
			distColor(randomEngine),
			1.0f,
			distColor(randomEngine),
			1.0f
		};
		particle.lifeTime = 9999.0f; // 一時的にずっと表示
		particle.velocity = { 0.0f, 0.0f, 0.0f }; // velocity は使用しない

		// 軌道パラメータを保存
		particle.orbitCenter = position;
		particle.orbitAxis = axis;
		particle.orbitRadius = radius;
		particle.orbitSpeed = 4.0f;
		particle.orbitPhase = t;
	}
	break;

	case ParticleEffectType::Explosion: // 爆発用
	{
		std::uniform_real_distribution<float> distDir(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distSpeed(5.0f, 12.0f);
		std::uniform_real_distribution<float> distScale(1.2f, 2.4f);
		std::uniform_real_distribution<float> distColor(0.7f, 1.0f);
		std::uniform_real_distribution<float> distTime(0.3f, 0.6f);

		// ランダムな方向に飛ばす
		Vector3 dir = {
			distDir(randomEngine),
			distDir(randomEngine),
			distDir(randomEngine)
		};
		dir = Vector3::Normalize(dir);
		float speed = distSpeed(randomEngine);

		// 設定
		particle.transform.translate_ = position;
		float scale = distScale(randomEngine);
		particle.transform.scale_ = { scale, scale, scale };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };

		particle.color = {
			distColor(randomEngine), distColor(randomEngine) * 0.4f, 0.0f, 1.0f
		}; // オレンジ系
		particle.lifeTime = distTime(randomEngine);
		particle.velocity = dir * speed;

		break;
	}
	case ParticleEffectType::Blood: // 血飛沫
	{
		std::uniform_real_distribution<float> distDir(-1.0f, 1.0f);
		std::uniform_real_distribution<float> distSpeed(3.0f, 7.0f);
		std::uniform_real_distribution<float> distLife(0.5f, 1.5f);

		Vector3 direction = {
			distDir(randomEngine),
			std::abs(distDir(randomEngine)),  // Yは上方向だけにする
			distDir(randomEngine)
		};

		// 正規化
		float len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if (len > 0.0f) {
			direction.x /= len;
			direction.y /= len;
			direction.z /= len;
		}

		particle.transform.translate_ = position;
		particle.transform.scale_ = { 1.0f, 1.0f, 1.0f };
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f };
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f };

		particle.color = { 1.0f, 0.0f, 0.0f, 1.0f };  // 赤
		particle.lifeTime = distLife(randomEngine);
		particle.velocity = {
			direction.x * distSpeed(randomEngine),
			direction.y * distSpeed(randomEngine),
			direction.z * distSpeed(randomEngine)
		};
		break;
	}
	case ParticleEffectType::LaserBeam: // レーザービーム
	{
		particle.transform.translate_ = position;
		particle.transform.scale_ = { 0.1f, 0.1f, 10.0f }; // Z方向に長い
		particle.startScale = particle.transform.scale_;
		particle.endScale = { 0.0f, 0.0f, 0.0f }; // 徐々に消える
		particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // 向きは外から設定するならあとで
		particle.color = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤いレーザー風
		particle.lifeTime = 0.1f; // 一瞬だけ表示
		particle.velocity = { 0.0f, 0.0f, 0.0f };
		break;
	}
	}

	particle.currentTime = 0.0f;
	return particle;
}


/// -------------------------------------------------------------
///				　		レーザービーム生成
/// -------------------------------------------------------------
Particle ParticleFactory::CreateLaserBeam(const Vector3& position, float length, const Vector3& color)
{
	Particle particle;
	particle.transform.translate_ = position;
	particle.transform.scale_ = { 0.1f, 0.1f, length }; // 長さZだけ動的に指定
	particle.startScale = particle.transform.scale_;
	particle.endScale = { 0.0f, 0.0f, 0.0f }; // 消える
	particle.transform.rotate_ = { 0.0f, 0.0f, 0.0f }; // 任意で向きも指定可能
	particle.color = { color.x, color.y, color.z, 1.0f };
	particle.lifeTime = 0.1f;
	particle.velocity = { 0.0f, 0.0f, 0.0f };
	particle.currentTime = 0.0f;
	return particle;
}
} // namespace Ken4lowEngine
