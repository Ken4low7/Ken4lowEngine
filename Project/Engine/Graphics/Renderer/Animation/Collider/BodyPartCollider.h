#pragma once
#include <string>
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// スケルトンのジョイントに紐づくボディパート当たり判定（デバッグ・ヒット判定用途）。
	/// - endJointIndex < 0 の場合は Sphere として扱う（startJointIndex + offset）
	/// - endJointIndex >= 0 の場合は Capsule（start→end）
	/// </summary>
	struct BodyPartCollider
	{
		std::string name;         // 名前（"LeftArm", "RightLeg", ...）
		int startJointIndex = -1; // 始点となるジョイント
		int endJointIndex = -1;   // 終点となるジョイント（カプセル用）
		Vector3 offset{};         // 単一ジョイント用のオフセット（sphere 描画等に使える）
		float radius = 0.1f;      // カプセルまたはスフィアの半径
		float height = 0.0f;      // offset を使う Capsule 用（レガシー用途 or fallback）
	};
} // namespace Ken4lowEngine
