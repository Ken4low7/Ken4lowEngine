#include "Matrix4x4.h"
#include "Vector3.h"
#include "Quaternion.h"
#include <limits>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///					　		平行移動成分の取得
	/// -------------------------------------------------------------
	Vector3 Matrix4x4::GetTranslation() const
	{
		return { m[3][0], m[3][1], m[3][2] };
	}

	/// -------------------------------------------------------------
	///					　　	コンストラクタ
	/// -------------------------------------------------------------
	Matrix4x4::Matrix4x4()
	{
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				m[i][j] = 0.0f;
			}
		}
	}

	/// -------------------------------------------------------------
	///					　　	コンストラクタ（配列版）
	/// -------------------------------------------------------------
	Matrix4x4::Matrix4x4(float elements[4][4])
	{
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				m[i][j] = elements[i][j];
			}
		}
	}

	/// -------------------------------------------------------------
	///					演算子オーバーロード
	/// -------------------------------------------------------------
	Matrix4x4& Matrix4x4::operator+=(const Matrix4x4& other)
	{
		// 加算の実装
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m[i][j] += other.m[i][j];
		return *this;
	}

	Matrix4x4& Matrix4x4::operator-=(const Matrix4x4& other)
	{
		// 減算の実装
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 4; ++j)
				m[i][j] -= other.m[i][j];
		return *this;
	}

	Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& other)
	{
		// 乗算の実装
		Matrix4x4 result;
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result.m[i][j] = 0;
				for (int k = 0; k < 4; ++k) {
					result.m[i][j] += m[i][k] * other.m[k][j];
				}
			}
		}
		*this = result;
		return *this;
	}

	Matrix4x4& Matrix4x4::operator=(const Matrix4x4& other)
	{
		if (this != &other) { // 自己代入チェック（重要！）
			std::memcpy(m, other.m, sizeof(float) * 4 * 4); // 4x4の行列コピー
		}
		return *this;
	}

	Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result = m1;
		result += m2;
		return result;
	}

	Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result = m1;
		result -= m2;
		return result;
	}

	Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result = m1;
		result *= m2;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　　基本演算（静的関数）
	/// -------------------------------------------------------------

	/// -------------------------------------------------------------
	///				　　　		行列の加算
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::Add(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result.m[i][j] = m1.m[i][j] + m2.m[i][j];
			}
		}
		return result;
	}

	/// -------------------------------------------------------------
	///				　　　		行列の減算
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::Subtract(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result.m[i][j] = m1.m[i][j] - m2.m[i][j];
			}
		}
		return result;
	}

	/// -------------------------------------------------------------
	///				　　　		行列の乗算
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::Multiply(const Matrix4x4& m1, const Matrix4x4& m2)
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				for (int k = 0; k < 4; k++)
				{
					result.m[i][j] += m1.m[i][k] * m2.m[k][j];
				}
			}
		}
		return result;
	}

	/// -------------------------------------------------------------
	///				　　		行列の逆行列
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::Inverse(const Matrix4x4& matrix)
	{
		Matrix4x4 result{};
		TryInverse(matrix, result);
		return result;
	}

	bool Matrix4x4::TryInverse(const Matrix4x4& matrix, Matrix4x4& out)
	{
		Matrix4x4 result{};

		float det
			= matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][1] + matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][2] - matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][1] - matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][3] - matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][2])
			- matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][0] - matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][2])
			+ matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][3] + matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][0] - matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][1])
			- matrix.m[0][3] * (matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][2] + matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][0] + matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][0] - matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][1]);

		constexpr float kEpsilon = 1.0e-12f;

		if (std::fabs(det) <= kEpsilon)
		{
			out = Matrix4x4::MakeIdentity();
			return false;
		}

		result.m[0][0] = (matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][1] + matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][2] - matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][1] - matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][3] - matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][2]) / det;
		result.m[0][1] = (-matrix.m[0][1] * matrix.m[2][2] * matrix.m[3][3] - matrix.m[0][2] * matrix.m[2][3] * matrix.m[3][1] - matrix.m[0][3] * matrix.m[2][1] * matrix.m[3][2] + matrix.m[0][3] * matrix.m[2][2] * matrix.m[3][1] + matrix.m[0][2] * matrix.m[2][1] * matrix.m[3][3] + matrix.m[0][1] * matrix.m[2][3] * matrix.m[3][2]) / det;
		result.m[0][2] = (matrix.m[0][1] * matrix.m[1][2] * matrix.m[3][3] + matrix.m[0][2] * matrix.m[1][3] * matrix.m[3][1] + matrix.m[0][3] * matrix.m[1][1] * matrix.m[3][2] - matrix.m[0][3] * matrix.m[1][2] * matrix.m[3][1] - matrix.m[0][2] * matrix.m[1][1] * matrix.m[3][3] - matrix.m[0][1] * matrix.m[1][3] * matrix.m[3][2]) / det;
		result.m[0][3] = (-matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][3] - matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][1] - matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][2] + matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][1] + matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][3] + matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][2]) / det;

		result.m[1][0] = (-matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][3] - matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][0] - matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][2] + matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][0] + matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][3] + matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][2]) / det;
		result.m[1][1] = (matrix.m[0][0] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[0][2] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[0][3] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[0][3] * matrix.m[2][2] * matrix.m[3][0] - matrix.m[0][2] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[0][0] * matrix.m[2][3] * matrix.m[3][2]) / det;
		result.m[1][2] = (-matrix.m[0][0] * matrix.m[1][2] * matrix.m[3][3] - matrix.m[0][2] * matrix.m[1][3] * matrix.m[3][0] - matrix.m[0][3] * matrix.m[1][0] * matrix.m[3][2] + matrix.m[0][3] * matrix.m[1][2] * matrix.m[3][0] + matrix.m[0][2] * matrix.m[1][0] * matrix.m[3][3] + matrix.m[0][0] * matrix.m[1][3] * matrix.m[3][2]) / det;
		result.m[1][3] = (matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][3] + matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][0] + matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][2] - matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][0] - matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][3] - matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][2]) / det;

		result.m[2][0] = (matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][3] + matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][0] - matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][1]) / det;
		result.m[2][1] = (-matrix.m[0][0] * matrix.m[2][1] * matrix.m[3][3] - matrix.m[0][1] * matrix.m[2][3] * matrix.m[3][0] - matrix.m[0][3] * matrix.m[2][0] * matrix.m[3][1] + matrix.m[0][3] * matrix.m[2][1] * matrix.m[3][0] + matrix.m[0][1] * matrix.m[2][0] * matrix.m[3][3] + matrix.m[0][0] * matrix.m[2][3] * matrix.m[3][1]) / det;
		result.m[2][2] = (matrix.m[0][0] * matrix.m[1][1] * matrix.m[3][3] + matrix.m[0][1] * matrix.m[1][3] * matrix.m[3][0] + matrix.m[0][3] * matrix.m[1][0] * matrix.m[3][1] - matrix.m[0][3] * matrix.m[1][1] * matrix.m[3][0] - matrix.m[0][1] * matrix.m[1][0] * matrix.m[3][3] - matrix.m[0][0] * matrix.m[1][3] * matrix.m[3][1]) / det;
		result.m[2][3] = (-matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][3] - matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][0] - matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][1] + matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][0] + matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][3] + matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][1]) / det;

		result.m[3][0] = (-matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][2] - matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][0] - matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][1] + matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][0] + matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][2] + matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][1]) / det;
		result.m[3][1] = (matrix.m[0][0] * matrix.m[2][1] * matrix.m[3][2] + matrix.m[0][1] * matrix.m[2][2] * matrix.m[3][0] + matrix.m[0][2] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[0][2] * matrix.m[2][1] * matrix.m[3][0] - matrix.m[0][1] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[0][0] * matrix.m[2][2] * matrix.m[3][1]) / det;
		result.m[3][2] = (-matrix.m[0][0] * matrix.m[1][1] * matrix.m[3][2] - matrix.m[0][1] * matrix.m[1][2] * matrix.m[3][0] - matrix.m[0][2] * matrix.m[1][0] * matrix.m[3][1] + matrix.m[0][2] * matrix.m[1][1] * matrix.m[3][0] + matrix.m[0][1] * matrix.m[1][0] * matrix.m[3][2] + matrix.m[0][0] * matrix.m[1][2] * matrix.m[3][1]) / det;
		result.m[3][3] = (matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][2] + matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][0] + matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][1] - matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][0] - matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][2] - matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][1]) / det;

		out = result;

		return true;
	}

	/// -------------------------------------------------------------
	///				　　		行列の転置
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::Transpose(const Matrix4x4& m)
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				result.m[i][j] = m.m[j][i];
			}
		}
		return result;
	}

	/// -------------------------------------------------------------
	///				　　	各種変換行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeIdentity()
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (i == j)
				{
					result.m[i][j] = 1.0f;
				}
				else
				{
					result.m[i][j] = 0.0f;
				}
			}
		}
		return result;
	}

	Matrix4x4 Matrix4x4::MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up)
	{
		// 前方向（左手座標系）
		Vector3 zAxis = Vector3::Normalize(target - eye);

		// 右方向
		Vector3 xAxis = Vector3::Normalize(Vector3::Cross(up, zAxis));

		// 上方向を再計算
		Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

		Matrix4x4 result{};

		result.m[0][0] = xAxis.x;
		result.m[0][1] = yAxis.x;
		result.m[0][2] = zAxis.x;
		result.m[0][3] = 0.0f;

		result.m[1][0] = xAxis.y;
		result.m[1][1] = yAxis.y;
		result.m[1][2] = zAxis.y;
		result.m[1][3] = 0.0f;

		result.m[2][0] = xAxis.z;
		result.m[2][1] = yAxis.z;
		result.m[2][2] = zAxis.z;
		result.m[2][3] = 0.0f;

		result.m[3][0] = -Vector3::Dot(xAxis, eye);
		result.m[3][1] = -Vector3::Dot(yAxis, eye);
		result.m[3][2] = -Vector3::Dot(zAxis, eye);
		result.m[3][3] = 1.0f;

		return result;
	}

	Matrix4x4 Matrix4x4::MakeLightViewProjection(const Vector3& lightDirection, const Vector3& target, float distanceFromTarget, float orthoHalfWidth, float orthoHalfHeight, float nearZ, float farZ)
	{
		Vector3 dir = Vector3::Normalize(lightDirection);
		Vector3 eye = target - dir * distanceFromTarget;
		// Directional Lightが真上/真下を向いてもLookAtのCross積が0にならないUp軸を選ぶ。
		Vector3 up = std::fabs(Vector3::Dot(dir, { 0.0f, 1.0f, 0.0f })) > 0.98f
			? Vector3{ 1.0f, 0.0f, 0.0f }
			: Vector3{ 0.0f, 1.0f, 0.0f };
		const float width = std::max(std::fabs(orthoHalfWidth), 0.01f);
		const float height = std::max(std::fabs(orthoHalfHeight), 0.01f);
		const float shadowNear = std::max(nearZ, 0.001f);
		const float shadowFar = std::max(farZ, shadowNear + 0.001f);

		Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(eye, target, up);
		Matrix4x4 proj = Matrix4x4::MakeOrthographicMatrix(-width, height, width, -height, shadowNear, shadowFar);

		return Matrix4x4::Multiply(view, proj);
	}

	/// -------------------------------------------------------------
	///				　　	拡大縮小行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeScaleMatrix(const Vector3& scale)
	{
		Matrix4x4 result{};
		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　	回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeRotateX(float radian)
	{
		Matrix4x4 result{};
		result.m[0][0] = 1.0f;
		result.m[1][1] = std::cos(radian);
		result.m[1][2] = std::sin(radian);
		result.m[2][1] = -std::sin(radian);
		result.m[2][2] = std::cos(radian);
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　	回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeRotateY(float radian)
	{
		Matrix4x4 result{};
		result.m[0][0] = std::cos(radian);
		result.m[0][2] = std::sin(radian);
		result.m[1][1] = 1.0f;
		result.m[2][0] = -std::sin(radian);
		result.m[2][2] = std::cos(radian);
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　	回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeRotateZMatrix(float radian)
	{
		Matrix4x4 result{};
		result.m[0][0] = std::cos(radian);
		result.m[0][1] = std::sin(radian);
		result.m[1][0] = -std::sin(radian);
		result.m[1][1] = std::cos(radian);
		result.m[2][2] = 1.0f;
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　		回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeRotateMatrix(const Vector3& radian)
	{
		return Multiply(Multiply(MakeRotateX(radian.x), MakeRotateY(radian.y)), MakeRotateZMatrix(radian.z));
	}

	/// -------------------------------------------------------------
	///				　　		平行移動行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeTranslateMatrix(const Vector3& translate)
	{
		Matrix4x4 result{};
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if (i == j)
				{
					result.m[i][j] = 1.0f;
				}
				else
				{
					result.m[i][j] = 0.0f;
				}
			}
		}
		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　	アフィン変換行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate)
	{
		return  Multiply(Multiply(MakeScaleMatrix(scale), MakeRotateMatrix(rotate)), MakeTranslateMatrix(translate));
	}

	/// -------------------------------------------------------------
	///			 アフィン変換行列の生成（クォータニオン版）
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate)
	{
		return Multiply(Multiply(MakeScaleMatrix(scale), Quaternion::MakeRotateMatrix(rotate)), MakeTranslateMatrix(translate));
	}

	/// -------------------------------------------------------------
	///				　　		透視投影行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
	{
		Matrix4x4 result{};
		result.m[0][0] = 1.0f / aspectRatio * 1.0f / tanf(fovY / 2.0f);
		result.m[1][1] = 1.0f / tanf(fovY / 2.0f);
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;
		result.m[3][2] = -farClip * nearClip / (farClip - nearClip);
		return result;
	}

	/// -------------------------------------------------------------
	///				　　		直交投影行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip)
	{
		Matrix4x4 result{};
		result.m[0][0] = 2 / (right - left);
		result.m[1][1] = 2 / (top - bottom);
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　ビューポート変換行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth)
	{
		Matrix4x4 result{};
		result.m[0][0] = width / 2.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[3][0] = left + width / 2.0f;
		result.m[3][1] = top + height / 2.0f;
		result.m[3][2] = minDepth;
		result.m[3][3] = 1.0f;
		return result;
	}

	/// -------------------------------------------------------------
	///				　　		軸回転行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::MakeRotateAxisAngleMatrix(const Vector3& axis, float angle)
	{
		Vector3 a = Vector3::Normalize(axis);

		float c = std::cos(angle);
		float s = std::sin(angle);
		float t = 1.0f - c;

		float x = a.x;
		float y = a.y;
		float z = a.z;

		Matrix4x4 result;
		result.m[0][0] = t * x * x + c;
		result.m[0][1] = t * x * y + s * z;
		result.m[0][2] = t * x * z - s * y;
		result.m[0][3] = 0.0f;

		result.m[1][0] = t * x * y - s * z;
		result.m[1][1] = t * y * y + c;
		result.m[1][2] = t * y * z + s * x;
		result.m[1][3] = 0.0f;

		result.m[2][0] = t * x * z + s * y;
		result.m[2][1] = t * y * z - s * x;
		result.m[2][2] = t * z * z + c;
		result.m[2][3] = 0.0f;

		result.m[3][0] = 0.0f;
		result.m[3][1] = 0.0f;
		result.m[3][2] = 0.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	/// -------------------------------------------------------------
	///				　　		行列の分解
	/// -------------------------------------------------------------
	void Matrix4x4::Decompose(const Matrix4x4& matrix, Vector3& outScale, Vector3& outRotate, Vector3& outTranslate)
	{
		// 平行移動成分
		outTranslate = { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };

		// スケール成分
		outScale.x = std::sqrt(matrix.m[0][0] * matrix.m[0][0] + matrix.m[0][1] * matrix.m[0][1] + matrix.m[0][2] * matrix.m[0][2]);
		outScale.y = std::sqrt(matrix.m[1][0] * matrix.m[1][0] + matrix.m[1][1] * matrix.m[1][1] + matrix.m[1][2] * matrix.m[1][2]);
		outScale.z = std::sqrt(matrix.m[2][0] * matrix.m[2][0] + matrix.m[2][1] * matrix.m[2][1] + matrix.m[2][2] * matrix.m[2][2]);

		// 回転成分（行列をスケール除去してからオイラー角に変換）
		Matrix4x4 rotMat = matrix;

		if (outScale.x != 0) { rotMat.m[0][0] /= outScale.x; rotMat.m[0][1] /= outScale.x; rotMat.m[0][2] /= outScale.x; }
		if (outScale.y != 0) { rotMat.m[1][0] /= outScale.y; rotMat.m[1][1] /= outScale.y; rotMat.m[1][2] /= outScale.y; }
		if (outScale.z != 0) { rotMat.m[2][0] /= outScale.z; rotMat.m[2][1] /= outScale.z; rotMat.m[2][2] /= outScale.z; }

		// オイラー角を求める (XYZ回転順)
		outRotate.y = std::asin(-rotMat.m[2][0]); // yaw
		if (std::cos(outRotate.y) != 0)
		{
			outRotate.x = std::atan2(rotMat.m[2][1], rotMat.m[2][2]); // pitch
			outRotate.z = std::atan2(rotMat.m[1][0], rotMat.m[0][0]); // roll
		}
		else {
			outRotate.x = std::atan2(-rotMat.m[1][2], rotMat.m[1][1]);
			outRotate.z = 0;
		}
	}

	/// -------------------------------------------------------------
	///				　　		ベクトルの変換
	/// -------------------------------------------------------------
	Vector3 Matrix4x4::Transform(const Vector3& v, const Matrix4x4& m)
	{
		Vector3 result;
		result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
		result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
		result.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
		return result;
	}

	/// -------------------------------------------------------------
	///				　　		LookAt行列の生成
	/// -------------------------------------------------------------
	Matrix4x4 Matrix4x4::LookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
	{
		// カメラの正面方向 (Z軸)
		Vector3 zAxis = Vector3::Normalize(target - eye);

		// カメラの右方向 (X軸)
		Vector3 xAxis = Vector3::Normalize(Vector3::Cross(up, zAxis));

		// カメラの上方向 (Y軸)
		Vector3 yAxis = Vector3::Cross(zAxis, xAxis);

		// 結果のビュー行列
		Matrix4x4 view = Matrix4x4::MakeIdentity();

		view.m[0][0] = xAxis.x;
		view.m[1][0] = xAxis.y;
		view.m[2][0] = xAxis.z;
		view.m[3][0] = -Vector3::Dot(xAxis, eye);

		view.m[0][1] = yAxis.x;
		view.m[1][1] = yAxis.y;
		view.m[2][1] = yAxis.z;
		view.m[3][1] = -Vector3::Dot(yAxis, eye);

		view.m[0][2] = zAxis.x;
		view.m[1][2] = zAxis.y;
		view.m[2][2] = zAxis.z;
		view.m[3][2] = -Vector3::Dot(zAxis, eye);

		view.m[0][3] = 0.0f;
		view.m[1][3] = 0.0f;
		view.m[2][3] = 0.0f;
		view.m[3][3] = 1.0f;

		return view;
	}

} // namespace Ken4lowEngine
