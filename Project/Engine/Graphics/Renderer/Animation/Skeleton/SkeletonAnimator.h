#pragma once
#include <map>
#include <string>
#include <vector>

#include "Skeleton.h"
#include "ModelData.h"
#include "AnimationSampler.h"
#include "SkinCluster.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　スケルトンアニメーションクラス
	/// -------------------------------------------------------------
	/// AnimationModel から以下の責務を切り出す：
	/// - Animation.nodeAnimations を joints に適用
	/// - Skeleton::UpdateSkeleton()
	/// - SkinCluster::UpdatePaletteMatrix()
	class SkeletonAnimator final
	{
	public:
		/// <summary>DebugSceneの負荷検証で、骨更新とパレット転送を分けて計測するための結果です。</summary>
		struct UpdateTimings
		{
			float skeletonMilliseconds = 0.0f;
			float paletteMilliseconds = 0.0f;
		};

		/// <summary>
		/// animation/time から Skeleton を更新し、必要なら SkinCluster のパレットも更新します。
		/// </summary>
		/// <param name="skeleton">更新対象スケルトン</param>
		/// <param name="animation">アニメーション</param>
		/// <param name="timeSeconds">アニメーション時刻（秒）</param>
		/// <param name="skinCluster">パレット更新したい場合のみ渡す（nullptr 可）</param>
		void Update(Skeleton& skeleton, const Animation& animation, float timeSeconds, SkinCluster* skinCluster = nullptr,
			UpdateTimings* timings = nullptr) const;

		void UpdateBlend(
			Skeleton& skeleton,
			const Animation& fromAnimation,
			float fromTimeSeconds,
			const Animation& toAnimation,
			float toTimeSeconds,
			float blendRate,
			SkinCluster* skinCluster = nullptr,
			UpdateTimings* timings = nullptr) const;
	};
}
