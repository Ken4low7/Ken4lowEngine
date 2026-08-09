#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///							四元数クラス
/// -------------------------------------------------------------
class Quaternion
{
public: /// ---------- メンバ変数 ---------- ///

	// 成分
	float x, y, z, w;

public:	/// ---------- コンストラクタ ---------- ///

	/// <summary>
	/// 単位四元数 (0, 0, 0, 1) で初期化します。
	/// 回転なしの状態を表します。
	/// </summary>
	Quaternion() : x(0), y(0), z(0), w(1) {}

	/// <summary>
	/// 指定した成分で四元数を初期化します。
	/// 通常は (x, y, z) が虚部、w が実部です。
	/// </summary>
	/// <param name="x">x 成分（虚部）</param>
	/// <param name="y">y 成分（虚部）</param>
	/// <param name="z">z 成分（虚部）</param>
	/// <param name="w">w 成分（実部）</param>
	Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

public: /// ---------- 静的メンバ関数 ---------- ///

	/// <summary>
	/// 2 つの四元数を掛け合わせます（回転の合成）
	/// </summary>
	/// <param name="lhs">左側の四元数</param>
	/// <param name="rhs">右側の四元数</param>
	/// <returns>lhs と rhs を合成した回転を表す四元数</returns>
	static Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs);

	/// <summary>
	/// 単位四元数 (0, 0, 0, 1) を返します。
	/// 回転なしの状態を表します。
	/// </summary>
	static Quaternion IdentityQuaternion();

	/// <summary>
	/// 共役四元数を返します。
	/// ベクトル部分 (x, y, z) の符号を反転し、w はそのままです。
	/// </summary>
	/// <param name="quaternion">対象の四元数</param>
	/// <returns>共役四元数</returns>
	static Quaternion Conjugate(const Quaternion& quaternion);

	/// <summary>
	/// 四元数のノルム（長さ）を返します。
	/// √(x² + y² + z² + w²) を計算します。
	/// </summary>
	/// <param name="quaternion">対象の四元数</param>
	/// <returns>ノルム（長さ）</returns>
	static float Norm(const Quaternion& quaternion);

	/// <summary>
	/// 四元数を正規化して長さ 1 の単位四元数を返します。
	/// ノルムが 0 の場合は単位四元数 (0,0,0,1) を返します。
	/// </summary>
	/// <param name="quaternion">正規化する四元数</param>
	/// <returns>正規化された四元数。</returns>
	static Quaternion Normalize(const Quaternion& quaternion);

	/// <summary>
	/// 四元数の逆数を返します。
	/// q^-1 = conjugate(q) / |q|² として計算します。
	/// ノルムが 0 の場合は単位四元数 (0,0,0,1) を返します。
	/// </summary>
	/// <param name="quaternion">対象の四元数</param>
	/// <returns>逆四元数。</returns>
	static Quaternion Inverse(const Quaternion& quaternion);

	/// <summary>
	/// 任意軸まわりの回転を表す四元数を生成します。
	/// </summary>
	/// <param name="axis">回転軸ベクトル。内部で正規化されます。</param>
	/// <param name="angle">
	/// 回転角。ラジアン単位を想定しています（例：π/2 で 90 度）
	/// </param>
	/// <returns>指定軸・角度の回転を表す四元数。</returns>
	static Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle);

	/// <summary>
	/// ベクトルを四元数の回転で回した結果を返します。
	/// v' = q * v * conj(q) を計算します。
	/// </summary>
	/// <param name="vector">回転させる 3D ベクトル</param>
	/// <param name="quaternion">適用する回転</param>
	/// <returns>回転後のベクトル</returns>
	static Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion);

	/// <summary>
	/// 四元数から回転行列を生成します。
	/// 行列のレイアウトは Matrix4x4 の実装に従います。
	/// </summary>
	/// <param name="quaternion">変換元の四元数</param>
	/// <returns>対応する 4x4 回転行列</returns>
	static Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

	/// <summary>
	/// 2 つの四元数の間を球面線形補間（Slerp）します。
	/// t = 0 で q0、t = 1 で q1 になります。
	/// 内積が負の場合は最短経路になるように自動で反転します。
	/// </summary>
	/// <param name="q0">補間開始の四元数</param>
	/// <param name="q1">補間終了の四元数</param>
	/// <param name="t">補間係数。通常 0.0f ～ 1.0f</param>
	/// <returns>t の位置に対応する補間結果の四元数</returns>
	static Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);
};

} // namespace Ken4lowEngine
