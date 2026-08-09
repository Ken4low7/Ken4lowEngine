#include "AnimationModelLODController.h"
#include <algorithm>

namespace Ken4lowEngine
{
	static int ClampInt(int v, int lo, int hi)
	{
		return (v < lo) ? lo : (v > hi) ? hi : v;
	}

	void AnimationModelLODController::ResetRuntimeState()
	{
		culledByDistance_ = false;
		lodIndex_ = 0;
		frame_ = 0;
	}

	void AnimationModelLODController::Reset()
	{
		thresholds_.clear();
		hysteresisGap_ = 0.0f;

		cullDistance_ = 200.0f;
		farCullExtra_ = 0.0f;

		heavyUpdateEveryByLOD_.clear();
		lodSwitchUpdateEvery_ = 1;

		forceLOD_ = false;
		forcedLODIndex_ = 0;

		ResetRuntimeState();
	}

	void AnimationModelLODController::SetThresholds(const std::vector<float>& thresholds)
	{
		thresholds_ = thresholds;
		std::sort(thresholds_.begin(), thresholds_.end());
	}

	void AnimationModelLODController::SetForceLOD(bool enable, int index)
	{
		forceLOD_ = enable;
		forcedLODIndex_ = index;
	}

	bool AnimationModelLODController::Update(float distance, int lodCount)
	{
		const float d = (distance < 0.0f) ? 0.0f : distance;
		return UpdateByDistanceSq(d * d, lodCount);
	}

	bool AnimationModelLODController::UpdateByDistanceSq(float distanceSq, int lodCount)
	{
		++frame_;

		const int prevLod = lodIndex_;
		const bool prevCull = culledByDistance_;

		// LODが無いなら安全側でカリング
		if (lodCount <= 0)
		{
			lodIndex_ = 0;
			culledByDistance_ = true;
			return (prevLod != lodIndex_) || (prevCull != culledByDistance_);
		}

		const int maxLod = lodCount - 1;

		// カリング判定は毎フレ更新
		const float cull = cullDistance_ + farCullExtra_;
		const float cullSq = (cull <= 0.0f) ? 0.0f : (cull * cull);
		culledByDistance_ = (distanceSq > cullSq);

		bool changed = (prevCull != culledByDistance_);

		// カリング中は最後LODに寄せる（任意：UAV等の参照を安定化）
		if (culledByDistance_)
		{
			const int newLod = ClampInt(maxLod, 0, maxLod);
			if (newLod != lodIndex_) { lodIndex_ = newLod; changed = true; }
			return changed;
		}

		// 強制LODは毎フレ即反映（切替間引きの対象外）
		if (forceLOD_)
		{
			const int newLod = ClampInt(forcedLODIndex_, 0, maxLod);
			if (newLod != lodIndex_) { lodIndex_ = newLod; changed = true; }
			return changed;
		}

		// LOD切替判定の間引き（※カリングは上で毎フレ更新済み）
		const uint32_t swEvery = (lodSwitchUpdateEvery_ == 0) ? 1u : lodSwitchUpdateEvery_;
		if ((frame_ % (uint64_t)swEvery) != 0)
		{
			return changed; // カリング変化があればそれだけ返す
		}

		// しきい値が無い場合は常にLOD0
		if (thresholds_.empty() || lodCount <= 1)
		{
			if (lodIndex_ != 0) { lodIndex_ = 0; changed = true; }
			return changed;
		}

		const int usable = std::min<int>((int)thresholds_.size(), lodCount - 1);

		// 現在LODを起点に、ヒステリシス込みで段階的に遷移させる
		int newLod = ClampInt(lodIndex_, 0, maxLod);

		// 遠ざかる方向（LOD index を増やす）：threshold + gap を超えたら段階UP
		while (newLod < usable)
		{
			const float t = thresholds_[newLod];
			const float in = t + hysteresisGap_;
			const float inSq = (in <= 0.0f) ? 0.0f : (in * in);
			if (distanceSq >= inSq) { ++newLod; }
			else { break; }
		}

		// 近づく方向（LOD index を減らす）：threshold - gap 未満なら段階DOWN
		while (newLod > 0)
		{
			const float t = thresholds_[newLod - 1];
			const float out = std::max(0.0f, t - hysteresisGap_);
			const float outSq = (out <= 0.0f) ? 0.0f : (out * out);
			if (distanceSq < outSq) { --newLod; }
			else { break; }
		}

		newLod = ClampInt(newLod, 0, maxLod);
		if (newLod != lodIndex_) { lodIndex_ = newLod; changed = true; }

		return changed;
	}

	bool AnimationModelLODController::ShouldDoHeavyUpdate() const
	{
		if (culledByDistance_) { return false; }

		uint32_t every = 1u;
		if (!heavyUpdateEveryByLOD_.empty())
		{
			const int idx = ClampInt(lodIndex_, 0, (int)heavyUpdateEveryByLOD_.size() - 1);
			every = heavyUpdateEveryByLOD_[idx];
			if (every == 0) { every = 1u; }
		}

		return (frame_ % (uint64_t)every) == 0;
	}
}
