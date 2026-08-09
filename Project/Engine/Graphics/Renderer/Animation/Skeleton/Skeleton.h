#pragma once
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"
#include "ModelData.h"

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <algorithm>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///		　				　スケルトンクラス
/// -------------------------------------------------------------
class Skeleton
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デフォルトコンストラクタ。<br/>
	/// ルートインデックスを -1 に初期化し、ジョイント配列／マップは空の状態になります。
	/// </summary>
	Skeleton() = default;

	/// <summary>
	/// ルートノード以下の階層構造からスケルトンを生成します。<br/>
	/// ・joints_ / jointMap_ / rootIndex_ を一度クリア<br/>
	/// ・CreateJointRecursive() を使って Node ツリーから Joint を再帰的に追加<br/>
	/// ・最後に name → index のマップ(jointMap_) を作成<br/>
	/// を行います。
	/// </summary>
	/// <param name="rootNode">スケルトンの元になるルート Node。</param>
	void CreateFromNode(const Node& rootNode);

	/// <summary>
	/// ルートノードから Skeleton を生成するヘルパー関数です。<br/>
	/// Skeleton インスタンスを new して CreateFromNode() を呼び出し、<br/>
	/// さらに UpdateSkeleton() まで実行した状態の unique_ptr を返します。
	/// </summary>
	/// <param name="rootNode">スケルトンの元になるルート Node。</param>
	/// <returns>生成済みスケルトンを保持する unique_ptr。</returns>
	static std::unique_ptr<Skeleton> CreateFromRootNode(const Node& rootNode);

	/// <summary>
	/// スケルトンの更新処理を行います。<br/>
	/// 各 Joint の Transform（scale / rotate / translate）からローカル行列を再計算し、<br/>
	/// 親から子へと再帰的にスケルトンスペース行列を更新します。<br/>
	/// ・joint.localMatrix = MakeAffineMatrix(scale, rotate, translate)<br/>
	/// ・joint.skeletonSpaceMatrix = local * parent.skeletonSpaceMatrix<br/>
	/// といった処理をルートから順に適用します。
	/// </summary>
	void UpdateSkeleton();

public: /// ---------- ゲッタ ---------- ///

	/// <summary>
	/// ジョイント配列への参照を取得します。<br/>
	/// スキンニングやデバッグ表示などで、直接 Joint 情報にアクセスしたい場合に使用します。
	/// </summary>
	/// <returns>Joint 配列への参照。</returns>
	std::vector<Joint>& GetJoints() { return joints_; }

	const std::vector<Joint>& GetJoints() const { return joints_; }

	/// <summary>
	/// ジョイント名からインデックスを引くためのマップを取得します。<br/>
	/// SkinCluster 側で「ジョイント名 → パレットインデックス」を解決する際などに使用されます。
	/// </summary>
	/// <returns>joint 名をキー、インデックスを値とするマップの const 参照。</returns>
	const std::map<std::string, int32_t>& GetJointMap() const { return jointMap_; }

	/// <summary>
	/// ルートジョイントのインデックスを取得します。<br/>
	/// joints_[GetRootIndex()] がスケルトン階層の根になります。
	/// </summary>
	/// <returns>ルートジョイントのインデックス。未設定時は -1。</returns>
	int32_t GetRootIndex() const { return rootIndex_; }

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// Node ツリーから Joint を再帰的に生成します。<br/>
	/// ・現在の Node から Joint を 1 つ作成し、joints_ に push_back<br/>
	/// ・parent 引数で親ジョイントのインデックスを受け取り、Joint::parent に設定<br/>
	/// ・各子 Node に対して再帰呼び出しを行い、children ベクタに子のインデックスを追加<br/>
	/// といった形で、階層構造を joints_ / children / parent に反映します。
	/// </summary>
	/// <param name="node">現在処理中の Node。</param>
	/// <param name="parent">親ジョイントのインデックス（ルートの場合 std::nullopt）。</param>
	/// <returns>作成した Joint のインデックス。</returns>
	uint32_t CreateJointRecursive(const Node& node, const std::optional<int32_t>& parent);


private: /// ---------- メンバ変数 ---------- ///

	int32_t rootIndex_ = -1; // ルートジョイントのIndex
	std::map<std::string, int32_t> jointMap_; // Joint名とIndexとの辞書
	std::vector<Joint> joints_; // 所属しているジョイント
};


} // namespace Ken4lowEngine
