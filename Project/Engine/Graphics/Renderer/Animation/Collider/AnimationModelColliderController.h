#pragma once
#include <vector>
#include <string>
#include <utility>

#include "BodyPartCollider.h"
#include "WorldTransform.h" 
#include "Capsule.h"
#include <Sphere.h>

namespace Ken4lowEngine
{
	class Skeleton;

	/// <summary>
	/// AnimationModel のボディパートコライダーを管理するコントローラ。
	/// - プリセット（Mixamo など）から生成
	/// - ワールド空間への変換（Capsule/Sphere）
	/// </summary>
	class AnimationModelColliderController
	{
	public:
		void Clear() { colliders_.clear(); }

		/// <summary>
		/// Mixamo リグ（mixamorig:*）前提のボディパートコライダーを生成します。
		/// </summary>
		void BuildMixamoHumanoid(const Skeleton& skeleton, float scaleFactor);

		const std::vector<BodyPartCollider>& GetColliders() const { return colliders_; }
		std::vector<BodyPartCollider>& GetCollidersRef() { return colliders_; }

		/// <summary>
		/// カプセル（＋スフィアをカプセル退化で）をワールド空間に変換して取得します。
		/// endJointIndex < 0 は origin==diff の退化カプセルとして返します。
		/// </summary>
		std::vector<std::pair<std::string, Capsule>> GetCapsulesWorld(const Skeleton& skeleton, const WorldTransform& worldTransform) const;

		/// <summary>
		/// スフィアのみ（endJointIndex < 0 の要素）をワールド空間に変換して取得します。
		/// </summary>
		std::vector<std::pair<std::string, Sphere>> GetSpheresWorld(const Skeleton& skeleton, const WorldTransform& worldTransform) const;

	private:
		std::vector<BodyPartCollider> colliders_;
	};
} // namespace Ken4lowEngine
