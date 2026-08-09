#include "AnimationModelColliderController.h"
#include "Skeleton.h"
#include "WorldTransform.h"

namespace Ken4lowEngine
{
	void AnimationModelColliderController::BuildMixamoHumanoid(const Skeleton& skeleton, float scaleFactor)
	{
		if (!&skeleton) return

		colliders_.clear();

		const auto& jointMap = skeleton.GetJointMap();

		/// ---------- 頭・首 ---------- ///
		if (auto it = jointMap.find("mixamorig:Head"); it != jointMap.end()) {
			colliders_.push_back({ "Head", it->second, -1, {0, 0.12f, 0}, 0.12f * scaleFactor, 0.0f });
		}
		if (auto it = jointMap.find("mixamorig:Neck"); it != jointMap.end()) {
			colliders_.push_back({ "Neck", it->second, -1, {0, 0.05f, 0}, 0.1f * scaleFactor, 0.0f });
		}

		/// ---------- 腹・胸 ---------- ///
		if (jointMap.contains("mixamorig:Spine") && jointMap.contains("mixamorig:Spine1")) {
			colliders_.push_back({ "SpineLower", jointMap.at("mixamorig:Spine"), jointMap.at("mixamorig:Spine1"), {}, 0.15f * scaleFactor, 0.0f });
		}
		if (jointMap.contains("mixamorig:Spine1") && jointMap.contains("mixamorig:Spine2")) {
			colliders_.push_back({ "SpineUpper", jointMap.at("mixamorig:Spine1"), jointMap.at("mixamorig:Spine2"), {0.0f,0.06f,0.0f}, 0.18f * scaleFactor, 0.0f });
		}

		/// ---------- 腰 ---------- ///
		if (auto it = jointMap.find("mixamorig:Hips"); it != jointMap.end()) {
			colliders_.push_back({ "Pelvis", it->second, -1, {}, 0.16f * scaleFactor, 0.0f });
		}

		/// ---------- 左上腕・左前腕 ---------- ///
		if (jointMap.contains("mixamorig:LeftArm") && jointMap.contains("mixamorig:LeftForeArm")) {
			colliders_.push_back({ "LeftUpperArm", jointMap.at("mixamorig:LeftArm"), jointMap.at("mixamorig:LeftForeArm"), {}, 0.1f * scaleFactor, 0.0f });
		}
		if (jointMap.contains("mixamorig:LeftForeArm") && jointMap.contains("mixamorig:LeftHand")) {
			colliders_.push_back({ "LeftLowerArm", jointMap.at("mixamorig:LeftForeArm"), jointMap.at("mixamorig:LeftHand"), {}, 0.09f * scaleFactor, 0.0f });
		}

		/// ---------- 右上腕・右前腕 ---------- ///
		if (jointMap.contains("mixamorig:RightArm") && jointMap.contains("mixamorig:RightForeArm")) {
			colliders_.push_back({ "RightUpperArm", jointMap.at("mixamorig:RightArm"), jointMap.at("mixamorig:RightForeArm"), {}, 0.1f * scaleFactor, 0.0f });
		}
		if (jointMap.contains("mixamorig:RightForeArm") && jointMap.contains("mixamorig:RightHand")) {
			colliders_.push_back({ "RightLowerArm", jointMap.at("mixamorig:RightForeArm"), jointMap.at("mixamorig:RightHand"), {}, 0.09f * scaleFactor, 0.0f });
		}

		/// ---------- 左大腿・左下腿 ---------- ///
		if (jointMap.contains("mixamorig:LeftUpLeg") && jointMap.contains("mixamorig:LeftLeg")) {
			colliders_.push_back({ "LeftUpperLeg", jointMap.at("mixamorig:LeftUpLeg"), jointMap.at("mixamorig:LeftLeg"), {}, 0.12f * scaleFactor, 0.0f });
		}
		if (jointMap.contains("mixamorig:LeftLeg") && jointMap.contains("mixamorig:LeftFoot")) {
			colliders_.push_back({ "LeftLowerLeg", jointMap.at("mixamorig:LeftLeg"), jointMap.at("mixamorig:LeftFoot"), {}, 0.1f * scaleFactor, 0.0f });
		}

		/// ---------- 右大腿・右下腿 ---------- ///
		if (jointMap.contains("mixamorig:RightUpLeg") && jointMap.contains("mixamorig:RightLeg")) {
			colliders_.push_back({ "RightUpperLeg", jointMap.at("mixamorig:RightUpLeg"), jointMap.at("mixamorig:RightLeg"), {}, 0.12f * scaleFactor, 0.0f });
		}
		if (jointMap.contains("mixamorig:RightLeg") && jointMap.contains("mixamorig:RightFoot")) {
			colliders_.push_back({ "RightLowerLeg", jointMap.at("mixamorig:RightLeg"), jointMap.at("mixamorig:RightFoot"), {}, 0.1f * scaleFactor, 0.0f });
		}

		/// ---------- 左足 ---------- ///
		if (jointMap.contains("mixamorig:LeftFoot") && jointMap.contains("mixamorig:LeftToeBase")) {
			colliders_.push_back({ "LeftToe", jointMap.at("mixamorig:LeftFoot"), jointMap.at("mixamorig:LeftToeBase"), {}, 0.07f * scaleFactor, 0.0f });
		}

		/// ---------- 右足 ---------- ///
		if (jointMap.contains("mixamorig:RightFoot") && jointMap.contains("mixamorig:RightToeBase")) {
			colliders_.push_back({ "RightToe", jointMap.at("mixamorig:RightFoot"), jointMap.at("mixamorig:RightToeBase"), {}, 0.07f * scaleFactor, 0.0f });
		}

		/// ---------- 左肩と右肩 ---------- ///
		if (auto it = jointMap.find("mixamorig:LeftShoulder"); it != jointMap.end()) {
			colliders_.push_back({ "LeftShoulder",it->second, -1, {-0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
		}
		if (auto it = jointMap.find("mixamorig:RightShoulder"); it != jointMap.end()) {
			colliders_.push_back({ "RightShoulder", it->second, -1, {0.08f, 0.0f, 0.0f}, 0.11f * scaleFactor, 0.0f });
		}

		/// ---------- 左腕・右腕 ---------- ///
		if (auto it = jointMap.find("mixamorig:LeftForeArm"); it != jointMap.end()) {
			colliders_.push_back({ "LeftElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
		}
		if (auto it = jointMap.find("mixamorig:RightForeArm"); it != jointMap.end()) {
			colliders_.push_back({ "RightElbow", it->second, -1, {}, 0.09f * scaleFactor, 0.0f });
		}

		/// ---------- 左膝・右膝 ---------- ///
		if (auto it = jointMap.find("mixamorig:LeftLeg"); it != jointMap.end()) {
			colliders_.push_back({ "LeftKnee", it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
		}
		if (auto it = jointMap.find("mixamorig:RightLeg"); it != jointMap.end()) {
			colliders_.push_back({ "RightKnee", it->second, -1, {}, 0.10f * scaleFactor, 0.0f });
		}

		/// ---------- 左手首・右手首 ---------- ///
		if (auto it = jointMap.find("mixamorig:LeftHand"); it != jointMap.end()) {
			colliders_.push_back({ "LeftWrist", it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
		}
		if (auto it = jointMap.find("mixamorig:RightHand"); it != jointMap.end()) {
			colliders_.push_back({ "RightWrist", it->second, -1, {}, 0.08f * scaleFactor, 0.0f });
		}
	}

	std::vector<std::pair<std::string, Capsule>> AnimationModelColliderController::GetCapsulesWorld(const Skeleton& skeleton, const WorldTransform& worldTransform) const
	{
		std::vector<std::pair<std::string, Capsule>> out;
		if (colliders_.empty()) { return out; }

		const auto& joints = skeleton.GetJoints();
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

		out.reserve(colliders_.size());
		for (const auto& part : colliders_)
		{
			// 念のため範囲チェック
			if (part.startJointIndex < 0 || part.startJointIndex >= (int)joints.size()) { continue; }
			if (part.endJointIndex >= (int)joints.size()) { continue; }

			Capsule capsule{};
			capsule.radius = part.radius;

			if (part.endJointIndex < 0)
			{
				const Vector3 local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
				const Vector3 world = Vector3::Transform(local, worldMatrix);
				capsule.segment.origin = capsule.segment.diff = world;
			}
			else
			{
				Vector3 a = Vector3::Transform(joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
				Vector3 b = Vector3::Transform(joints[part.endJointIndex].skeletonSpaceMatrix.GetTranslation(), worldMatrix);
				capsule.segment.origin = a;
				capsule.segment.diff = b;
			}

			out.emplace_back(part.name, capsule);
		}

		return out;
	}

	std::vector<std::pair<std::string, Sphere>> AnimationModelColliderController::GetSpheresWorld(const Skeleton& skeleton, const WorldTransform& worldTransform) const
	{
		std::vector<std::pair<std::string, Sphere>> out;
		if (colliders_.empty()) { return out; }

		const auto& joints = skeleton.GetJoints();
		Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale_, worldTransform.rotate_, worldTransform.translate_);

		for (const auto& part : colliders_)
		{
			if (part.endJointIndex >= 0) { continue; }
			if (part.startJointIndex < 0 || part.startJointIndex >= (int)joints.size()) { continue; }

			Sphere s{};
			Vector3 local = joints[part.startJointIndex].skeletonSpaceMatrix.GetTranslation() + part.offset;
			s.center = Vector3::Transform(local, worldMatrix);
			s.radius = part.radius;
			out.emplace_back(part.name, s);
		}

		return out;
	}
} // namespace Ken4lowEngine
