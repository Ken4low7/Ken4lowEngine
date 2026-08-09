#pragma once
#include "DX12Include.h"
#include <vector>
#include <map>
#include <optional>
#include "VertexData.h"
#include "Material.h"
#include "Quaternion.h"
#include "Matrix4x4.h"
#include <span>
#include <array>

namespace Ken4lowEngine
{

// EulerTransformの構造体
struct EulerTransform
{
	Vector3 scale{};
	Vector3 rotate{};	// Eulerでの回転
	Vector3 trnaslate{};
};

// QuaternionTransformの構造体
struct QuaternionTransform
{
	Vector3 scale{};
	Quaternion rotate{};
	Vector3 translate{};
};

// キーフレームの構造体
template <typename T>
struct Keyframe
{
	float time = 0.0f; // 時刻
	T value{};	// 値
};

using KeyframeVector3 = Keyframe<Vector3>;		 // Vector3のキーフレーム
using KeyframeQuaternion = Keyframe<Quaternion>; // Quaternionのキーフレーム

// NodeのAnimationの構造体
struct NodeAnimation
{
	std::vector<KeyframeVector3> translate;
	std::vector<KeyframeQuaternion> rotate;
	std::vector<KeyframeVector3> scale;
};

// 拡張する場合のテンプレート
template <typename T>
struct AnimationCurve
{
	std::vector<T> keyflames;
};

// アニメーションを表現する構造体
struct Animation
{
	float duration = 0.0f; // アニメーション全体の尺（単位は秒）
	// NodeAnimationの集合、Node名でひけるようにしておく
	std::map<std::string, NodeAnimation> nodeAnimations = {}; // Node名をキーにしてNodeAnimationを格納
};

// jointの構造体
struct Joint
{
	QuaternionTransform transform; // Transform情報
	Matrix4x4 localMatrix;		   // localMatrix
	Matrix4x4 skeletonSpaceMatrix; // skeletonSpaceでの変換行列
	std::string name;			   // 名前
	std::vector<int32_t> children; // 子JointのIndexのリスト。居なければ空
	int32_t index = 0;			   // 自身のindex
	std::optional<int32_t> parent; // 親JointのIndex。いなければnull
};

// ノード
struct Node
{
	QuaternionTransform transform;
	Matrix4x4 localMatrix{};
	std::string name;
	std::vector<Node> children;
};

// VertexWeightDataの構造体
struct VertexWeightData
{
	float weight;
	uint32_t vertexIndex;
};

// JointWeightDataの構造体
struct JointWeightData
{
	Matrix4x4 inverseBindPoseMatrix;
	std::vector<VertexWeightData> vertexWeights;
};

// サブメッシュの構造体
struct SubMesh
{
	std::vector<VertexData> vertices; // 頂点リスト
	std::vector<uint32_t> indices; // インデックスリスト
	Material material;			 // マテリアル
};

// ModelData構造体
struct ModelData
{
	std::map<std::string, JointWeightData> skinClusterData;
	std::vector<SubMesh> subMeshes;
	Node rootNode;
};

// 最大4Jointの影響を受ける
const uint32_t kNumMaxInfluence = 4;

// インフルエンスの構造体
struct VertexInfluence
{
	std::array<float, kNumMaxInfluence> weights;
	std::array<int32_t, kNumMaxInfluence> jointIndices;
};

// WellForGPUの構造体
struct WellForGPU
{
	Matrix4x4 skeletonSpaceMatrix; // 位置用
	Matrix4x4 skeletonSpaceInverceTransposeMatrix; // 法線用
};

struct SkinningInformationForGPU
{
	uint32_t numVertices; // 頂点数
	bool isSkinning;    // スキニングするか
};
} // namespace Ken4lowEngine
