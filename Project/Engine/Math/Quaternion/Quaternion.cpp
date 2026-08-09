#include "Quaternion.h"
#include <corecrt_math.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///					　		四元数の掛け算
	/// -------------------------------------------------------------
	Quaternion Quaternion::Multiply(const Quaternion& lhs, const Quaternion& rhs)
	{
		// 四元数の掛け算を計算
		Quaternion result;
		result.w = lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
		result.x = lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y;
		result.y = lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x;
		result.z = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w;
		return result;
	}

	/// -------------------------------------------------------------
	///					　		単位四元数の取得
	/// -------------------------------------------------------------
	Quaternion Quaternion::IdentityQuaternion()
	{
		// 単位四元数を返す
		return { 0.0f, 0.0f, 0.0f, 1.0f };
	}

	/// -------------------------------------------------------------
	///					　		共役四元数の取得
	/// -------------------------------------------------------------
	Quaternion Quaternion::Conjugate(const Quaternion& quaternion)
	{
		// 共役四元数を計算
		return { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };
	}

	/// -------------------------------------------------------------
	///					　		ノルム（長さ）の取得
	/// -------------------------------------------------------------
	float Quaternion::Norm(const Quaternion& quaternion)
	{
		// ノルム（長さ）を計算
		return sqrtf(quaternion.x * quaternion.x + quaternion.y * quaternion.y + quaternion.z * quaternion.z + quaternion.w * quaternion.w);
	}

	/// -------------------------------------------------------------
	///					　		正規化
	/// -------------------------------------------------------------
	Quaternion Quaternion::Normalize(const Quaternion& quaternion)
	{
		// 正規化を計算
		float norm = Norm(quaternion);

		// ノルムが0の場合、デフォルトの単位Quaternionを返す
		if (norm == 0.0f) return { 0.0f, 0.0f, 0.0f, 1.0f }; // デフォルトの単位Quaternionを返す

		// 正規化された四元数を返す
		return { quaternion.x / norm, quaternion.y / norm, quaternion.z / norm, quaternion.w / norm };
	}

	/// -------------------------------------------------------------
	///					　		逆四元数の取得
	/// -------------------------------------------------------------
	Quaternion Quaternion::Inverse(const Quaternion& quaternion)
	{
		// 逆四元数を計算
		float norm = Norm(quaternion);

		// ノルムが0の場合、デフォルトの単位Quaternionを返す
		if (norm == 0.0f) return { 0.0f, 0.0f, 0.0f, 1.0f }; // デフォルトの単位Quaternionを返す

		// 逆四元数の計算
		Quaternion conjugate = Conjugate(quaternion);

		// 逆四元数を返す
		return { conjugate.x / (norm * norm), conjugate.y / (norm * norm), conjugate.z / (norm * norm), conjugate.w / (norm * norm) };
	}

	/// -------------------------------------------------------------
	///					任意軸まわりの回転四元数生成
	/// -------------------------------------------------------------
	Quaternion Quaternion::MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle)
	{
		// 任意軸まわりの回転を表す四元数を生成
		Quaternion result;
		float halfAngle = angle / 2.0f;
		float sinHalfAngle = sinf(halfAngle);

		// 回転軸を正規化
		Vector3 normalizedAxis = Vector3::Normalize(axis);

		// クォータニオンの成分を計算
		result.x = normalizedAxis.x * sinHalfAngle;
		result.y = normalizedAxis.y * sinHalfAngle;
		result.z = normalizedAxis.z * sinHalfAngle;
		result.w = cosf(halfAngle);

		return result;
	}

	/// -------------------------------------------------------------
	///					ベクトルの回転
	/// -------------------------------------------------------------
	Vector3 Quaternion::RotateVector(const Vector3& vector, const Quaternion& quaternion)
	{
		// 回転計算の安定化のため、入力四元数は正規化してから使用する
		const Quaternion qNorm = Normalize(quaternion);

		// ベクトルをクォータニオン形式に変換
		const Quaternion qVector = { vector.x, vector.y, vector.z, 0.0f };

		// 正規化済みクォータニオンの共役を取得
		const Quaternion qConjugate = Conjugate(qNorm);

		// クォータニオンの掛け算を用いて回転を適用
		const Quaternion qResult = Multiply(Multiply(qNorm, qVector), qConjugate);

		// 回転後のベクトルを取得
		return { qResult.x, qResult.y, qResult.z };
	}

	/// -------------------------------------------------------------
	///					四元数から回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Quaternion::MakeRotateMatrix(const Quaternion& quaternion)
	{
		Matrix4x4 result;

		float xx = quaternion.x * quaternion.x;
		float yy = quaternion.y * quaternion.y;
		float zz = quaternion.z * quaternion.z;
		float ww = quaternion.w * quaternion.w;
		float xy = quaternion.x * quaternion.y;
		float xz = quaternion.x * quaternion.z;
		float yz = quaternion.y * quaternion.z;
		float wx = quaternion.w * quaternion.x;
		float wy = quaternion.w * quaternion.y;
		float wz = quaternion.w * quaternion.z;

		result.m[0][0] = ww + xx - yy - zz;
		result.m[0][1] = 2.0f * (xy + wz);
		result.m[0][2] = 2.0f * (xz - wy);
		result.m[0][3] = 0.0f;

		result.m[1][0] = 2.0f * (xy - wz);
		result.m[1][1] = ww - xx + yy - zz;
		result.m[1][2] = 2.0f * (yz + wx);
		result.m[1][3] = 0.0f;

		result.m[2][0] = 2.0f * (xz + wy);
		result.m[2][1] = 2.0f * (yz - wx);
		result.m[2][2] = ww - xx - yy + zz;
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	/// -------------------------------------------------------------
	///					球面線形補間（Slerp）
	/// -------------------------------------------------------------
	Quaternion Quaternion::Slerp(const Quaternion& q0, const Quaternion& q1, float t)
	{
		// クォータニオンの内積を計算
		float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;

		// 内積が負の場合、反転させて最短経路を取るようにする
		Quaternion q1Copy = q1;
		if (dot < 0.0f) {
			q1Copy.x = -q1.x; // q1を反転
			q1Copy.y = -q1.y;
			q1Copy.z = -q1.z;
			q1Copy.w = -q1.w;

			dot = -dot; // 内積も反転
		}

		// 内積が非常に大きい場合、線形補間を行う
		const float epsilon = 0.0005f;
		Quaternion result{};
		if (dot > 1.0f - epsilon) {
			// 線形補間
			result.x = q0.x + t * (q1Copy.x - q0.x);
			result.y = q0.y + t * (q1Copy.y - q0.y);
			result.z = q0.z + t * (q1Copy.z - q0.z);
			result.w = q0.w + t * (q1Copy.w - q0.w);
			return Normalize(result);  // 正規化して返す
		}

		// θ (theta) を求める
		float theta = acosf(dot);

		// theta と sin(θ) を使って補間係数scale0, scale1 を求める
		float sinTheta = sin(theta);
		float a = sin((1.0f - t) * theta) / sinTheta;
		float b = sin(t * theta) / sinTheta;

		// それぞれの補間係数を利用して補間後のQuaternionを求める
		result.x = a * q0.x + b * q1Copy.x;
		result.y = a * q0.y + b * q1Copy.y;
		result.z = a * q0.z + b * q1Copy.z;
		result.w = a * q0.w + b * q1Copy.w;

		return result;
	}

} // namespace Ken4lowEngine
