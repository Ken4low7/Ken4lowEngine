#include "SkeletonAnimator.h"

#include "LinearInterpolation.h"

#include <algorithm>
#include <chrono>

namespace Ken4lowEngine
{
	namespace
	{
		const NodeAnimation* FindNodeAnimation(const Animation& animation, const std::string& jointName)
		{
			const auto it = animation.nodeAnimations.find(jointName);
			return it == animation.nodeAnimations.end() ? nullptr : &it->second;
		}

		Vector3 SampleTranslateOrFallback(const NodeAnimation* nodeAnimation, float timeSeconds, const Vector3& fallback)
		{
			if (!nodeAnimation || nodeAnimation->translate.empty()) { return fallback; }
			return AnimationSampler::CalculateValue(nodeAnimation->translate, timeSeconds);
		}

		Quaternion SampleRotateOrFallback(const NodeAnimation* nodeAnimation, float timeSeconds, const Quaternion& fallback)
		{
			if (!nodeAnimation || nodeAnimation->rotate.empty()) { return fallback; }
			return AnimationSampler::CalculateValue(nodeAnimation->rotate, timeSeconds);
		}

		Vector3 SampleScaleOrFallback(const NodeAnimation* nodeAnimation, float timeSeconds, const Vector3& fallback)
		{
			if (!nodeAnimation || nodeAnimation->scale.empty()) { return fallback; }
			return AnimationSampler::CalculateValue(nodeAnimation->scale, timeSeconds);
		}
	}

	void SkeletonAnimator::Update(Skeleton& skeleton, const Animation& animation, float timeSeconds, SkinCluster* skinCluster,
		UpdateTimings* timings) const
	{
		const auto skeletonBegin = std::chrono::steady_clock::now();
		auto& joints = skeleton.GetJoints();
		if (joints.empty())
		{
			return;
		}

		auto& nodeAnimations = animation.nodeAnimations;

		// ノードアニメーションの適用
		for (Joint& joint : joints)
		{
			auto it = nodeAnimations.find(joint.name);
			if (it == nodeAnimations.end())
			{
				continue;
			}

			const NodeAnimation& nodeAnim = it->second;

			// AnimationSampler は keyframes が空だと assert するので、空チェックしてから呼ぶ
			if (!nodeAnim.translate.empty())
			{
				joint.transform.translate = AnimationSampler::CalculateValue(nodeAnim.translate, timeSeconds);
			}
			if (!nodeAnim.rotate.empty())
			{
				joint.transform.rotate = AnimationSampler::CalculateValue(nodeAnim.rotate, timeSeconds);
			}
			if (!nodeAnim.scale.empty())
			{
				joint.transform.scale = AnimationSampler::CalculateValue(nodeAnim.scale, timeSeconds);
			}

			// ローカル行列を更新（AnimationModel の現挙動を維持）
			joint.localMatrix = Matrix4x4::MakeAffineMatrix(
				joint.transform.scale,
				joint.transform.rotate,
				joint.transform.translate
			);
		}

		// スケルトン更新
		skeleton.UpdateSkeleton();
		if (timings)
		{
			timings->skeletonMilliseconds =
				std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - skeletonBegin).count();
		}

		// パレット更新（必要なときだけ）
		if (skinCluster)
		{
			const auto paletteBegin = std::chrono::steady_clock::now();
			skinCluster->UpdatePaletteMatrix(skeleton);
			if (timings)
			{
				timings->paletteMilliseconds =
					std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - paletteBegin).count();
			}
		}
	}

	void SkeletonAnimator::UpdateBlend(
		Skeleton& skeleton,
		const Animation& fromAnimation,
		float fromTimeSeconds,
		const Animation& toAnimation,
		float toTimeSeconds,
		float blendRate,
		SkinCluster* skinCluster,
		UpdateTimings* timings) const
	{
		const auto skeletonBegin = std::chrono::steady_clock::now();
		auto& joints = skeleton.GetJoints();
		if (joints.empty()) { return; }

		const float t = std::clamp(blendRate, 0.0f, 1.0f);
		for (Joint& joint : joints)
		{
			const NodeAnimation* fromNodeAnimation = FindNodeAnimation(fromAnimation, joint.name);
			const NodeAnimation* toNodeAnimation = FindNodeAnimation(toAnimation, joint.name);
			if (!fromNodeAnimation && !toNodeAnimation)
			{
				continue;
			}

			const Vector3 fromTranslate = SampleTranslateOrFallback(fromNodeAnimation, fromTimeSeconds, joint.transform.translate);
			const Vector3 toTranslate = SampleTranslateOrFallback(toNodeAnimation, toTimeSeconds, fromTranslate);
			const Quaternion fromRotate = SampleRotateOrFallback(fromNodeAnimation, fromTimeSeconds, joint.transform.rotate);
			const Quaternion toRotate = SampleRotateOrFallback(toNodeAnimation, toTimeSeconds, fromRotate);
			const Vector3 fromScale = SampleScaleOrFallback(fromNodeAnimation, fromTimeSeconds, joint.transform.scale);
			const Vector3 toScale = SampleScaleOrFallback(toNodeAnimation, toTimeSeconds, fromScale);

			joint.transform.translate = Lerp(fromTranslate, toTranslate, t);
			joint.transform.rotate = Quaternion::Slerp(fromRotate, toRotate, t);
			joint.transform.scale = Lerp(fromScale, toScale, t);
			joint.localMatrix = Matrix4x4::MakeAffineMatrix(
				joint.transform.scale,
				joint.transform.rotate,
				joint.transform.translate
			);
		}

		skeleton.UpdateSkeleton();
		if (timings)
		{
			timings->skeletonMilliseconds =
				std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - skeletonBegin).count();
		}

		if (skinCluster)
		{
			const auto paletteBegin = std::chrono::steady_clock::now();
			skinCluster->UpdatePaletteMatrix(skeleton);
			if (timings)
			{
				timings->paletteMilliseconds =
					std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - paletteBegin).count();
			}
		}
	}
}
