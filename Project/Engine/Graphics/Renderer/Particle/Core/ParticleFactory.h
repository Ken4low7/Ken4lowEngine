#pragma once
#include <random>
#include "Vector3.h"
#include "Particle.h"
#include "ParticleEffectType.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///						パーティクル生成クラス
/// -------------------------------------------------------------
class ParticleFactory
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定された乱数エンジン、位置、およびエフェクト種別に基づいて Particle を生成する静的メソッド。
	/// </summary>
	/// <param name="randomEngine">パーティクル生成時のランダムな値を供給する std::mt19937 の参照。</param>
	/// <param name="position">生成するパーティクルの位置を表す Vector3 型の参照（読み取り専用）。</param>
	/// <param name="effectType">生成するパーティクルに適用するエフェクトの種類を表す ParticleEffectType。</param>
	/// <returns>生成された Particle オブジェクト。</returns>
	static Particle Create(std::mt19937& randomEngine, const Vector3& position, ParticleEffectType effectType);

	/// <summary>
	/// 指定した位置、長さ、色でレーザービームを表す Particle を作成します。
	/// </summary>
	/// <param name="position">レーザービームの開始位置を表すベクトル（ワールド座標）。</param>
	/// <param name="length">レーザービームの長さ。</param>
	/// <param name="color">レーザービームの色を表すベクトル（例：RGB）。</param>
	/// <returns>作成された Particle オブジェクト。レーザービームを表します。</returns>
	static Particle CreateLaserBeam(const Vector3& position, float length, const Vector3& color);
};


} // namespace Ken4lowEngine
