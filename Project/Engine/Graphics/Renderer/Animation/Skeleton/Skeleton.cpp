#include "Skeleton.h"
#include <functional>
#include "Wireframe.h"
#include "DirectXCommon.h"
#include <numeric>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///				　		ノードからスケルトンを生成
/// -------------------------------------------------------------
void Skeleton::CreateFromNode(const Node& rootNode)
{
	joints_.clear();
	jointMap_.clear();
	rootIndex_ = CreateJointRecursive(rootNode, std::nullopt);

	// 名前とインデックスのマップを作成
	for (const Joint& joint : joints_)
	{
		jointMap_.emplace(joint.name, joint.index);
	}
}

/// -------------------------------------------------------------
///				　	ルートノードからスケルトンを生成
/// -------------------------------------------------------------
std::unique_ptr<Skeleton> Skeleton::CreateFromRootNode(const Node& rootNode)
{
	auto skeleton = std::make_unique<Skeleton>();
	skeleton->CreateFromNode(rootNode); // 再利用！
	skeleton->UpdateSkeleton(); // スケルトンの更新
	return skeleton;
}

/// -------------------------------------------------------------
///				　　 　スケルトンの更新処理
/// -------------------------------------------------------------
void Skeleton::UpdateSkeleton()
{
	// ジョイントが空なら何もしない
	if (joints_.empty()) return;

	// 再帰的にジョイントを更新するラムダ関数
	std::function<void(uint32_t)> updateJoint = [&](uint32_t index)
		{
			Joint& joint = joints_[index];

			// ローカル行列を取得
			joint.localMatrix = Matrix4x4::MakeAffineMatrix(joint.transform.scale, joint.transform.rotate, joint.transform.translate);

			if (joint.parent.has_value())
			{
				// 親の行列を取得してスケルトンスペース行列を更新
				joint.skeletonSpaceMatrix = joint.localMatrix * joints_[*joint.parent].skeletonSpaceMatrix;
			}
			else
			{
				// 親の行列を取得できなかったら
				joint.skeletonSpaceMatrix = joint.localMatrix;

			}

			// 子どもの行列を更新
			for (int32_t childIndex : joint.children)
			{
				updateJoint(childIndex);
			}
		};

	// ルートジョイントから更新を開始
	updateJoint(rootIndex_);
}

/// -------------------------------------------------------------
///				　　再帰的にジョイントを作成
/// -------------------------------------------------------------
uint32_t Skeleton::CreateJointRecursive(const Node& node, const std::optional<int32_t>& parent)
{
	// 現在のジョイントのインデックスを取得
	int32_t currentIndex = static_cast<int32_t>(joints_.size());

	// ジョイントを作成して追加
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = Matrix4x4::MakeIdentity(); // 初期値
	joint.transform = node.transform;
	joint.index = currentIndex;
	joint.parent = parent;
	joints_.push_back(joint);
	jointMap_[joint.name] = currentIndex;

	// 子ノードに対して再帰的にジョイントを作成
	for (const auto& childNode : node.children)
	{
		int32_t childIndex = CreateJointRecursive(childNode, currentIndex);
		joints_[currentIndex].children.push_back(childIndex);
	}

	// 現在のジョイントのインデックスを返す
	return currentIndex;
}

} // namespace Ken4lowEngine
