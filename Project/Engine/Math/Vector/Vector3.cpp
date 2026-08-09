#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///					　		加算
	/// -------------------------------------------------------------
	Vector3 Vector3::Add(const Vector3& v1, const Vector3& v2)
	{
		Vector3 result{};
		result.x = v1.x + v2.x;
		result.y = v1.y + v2.y;
		result.z = v1.z + v2.z;
		return result;
	}

	/// -------------------------------------------------------------
	///					　		減算
	/// -------------------------------------------------------------
	Vector3 Vector3::Subtract(const Vector3& v1, const Vector3& v2)
	{
		Vector3 result{};
		result.x = v1.x - v2.x;
		result.y = v1.y - v2.y;
		result.z = v1.z - v2.z;
		return result;
	}

	/// -------------------------------------------------------------
	///					　		スカラー倍
	/// -------------------------------------------------------------
	Vector3 Vector3::Multiply(float scalar, const Vector3& v)
	{
		Vector3 result{};
		result.x = scalar * v.x;
		result.y = scalar * v.y;
		result.z = scalar * v.z;
		return result;
	}

	/// -------------------------------------------------------------
	///					　		内積
	/// -------------------------------------------------------------
	float Vector3::Dot(const Vector3& v1, const Vector3& v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	/// -------------------------------------------------------------
	///					　		長さ（ノルム）
	/// -------------------------------------------------------------
	float Vector3::Length(const Vector3& v)
	{
		return sqrtf(powf(v.x, 2) + powf(v.y, 2) + powf(v.z, 2));
	}

	float Vector3::LengthXZ(const Vector3& v)
	{
		return std::sqrt(v.x * v.x + v.z * v.z);
	}

	float Vector3::LengthSquared(const Vector3& v)
	{
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}

	/// -------------------------------------------------------------
	///					　		正規化
	/// -------------------------------------------------------------
	Vector3 Vector3::Normalize(const Vector3& v)
	{
		return NormalizeSafe(v);
	}

	Vector3 Vector3::NormalizeXZ(const Vector3& v)
	{
		const float len = LengthXZ(v);
		if (len <= 1e-6f) return { 0.0f, 0.0f, 0.0f };
		return { v.x / len, 0.0f, v.z / len };
	}

	Vector3 Vector3::NormalizeSafe(const Vector3& v, const Vector3& fallback)
	{
		constexpr float kEpsilon = 1.0e-6f;

		const float lengthSq = LengthSquared(v);
		if (lengthSq <= kEpsilon * kEpsilon)
		{
			return fallback;
		}

		const float invLength = 1.0f / std::sqrt(lengthSq);
		return {
			v.x * invLength,
			v.y * invLength,
			v.z * invLength
		};
	}

	
	/// -------------------------------------------------------------
	///					座標変換（4x4行列）
	/// -------------------------------------------------------------
	Vector3 Vector3::Transform(const Vector3& vector, const Matrix4x4& matrix)
	{
		Vector3 result{};
		result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + 1.0f * matrix.m[3][0];
		result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + 1.0f * matrix.m[3][1];
		result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + 1.0f * matrix.m[3][2];
		float  w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + 1.0f * matrix.m[3][3];
		if (w == 0.0f) w = 1.0f; // w除算対策
		result.x /= w;
		result.y /= w;
		result.z /= w;
		return result;
	}

	/// -------------------------------------------------------------
	///					　		クロス積
	/// -------------------------------------------------------------
	Vector3 Vector3::Cross(const Vector3& v1, const Vector3& v2)
	{
		Vector3 result{};
		result.x = v1.y * v2.z - v1.z * v2.y;
		result.y = v1.z * v2.x - v1.x * v2.z;
		result.z = v1.x * v2.y - v1.y * v2.x;
		return result;
	}

	/// -------------------------------------------------------------
	///					Catmull-Rom スプライン補間
	/// -------------------------------------------------------------
	Vector3 Vector3::CatmullRomSpline(const Vector3& P0, const Vector3& P1, const Vector3& P2, const Vector3& P3, float t)
	{
		float t2 = t * t;
		float t3 = t2 * t;

		return 0.5f * (
			(2.0f * P1) +
			(-P0 + P2) * t +
			(2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * t2 +
			(-P0 + 3.0f * P1 - 3.0f * P2 + P3) * t3
			);
	}

	/// -------------------------------------------------------------
	///				　　　演算子オーバーロード
	/// -------------------------------------------------------------
	Vector3 Vector3::operator+() const { return *this; }
	Vector3 Vector3::operator-() const { return Vector3(-x, -y, -z); }

	/// -------------------------------------------------------------
	///				　　	複合代入演算子
	/// -------------------------------------------------------------
	Vector3& Vector3::operator+=(const Vector3& other)
	{
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}

	Vector3& Vector3::operator-=(const Vector3& other)
	{
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}

	Vector3& Vector3::operator*=(float s)
	{
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}

	Vector3& Vector3::operator/=(float s)
	{
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}

	/// -------------------------------------------------------------
	///				　　	等価比較演算子
	/// -------------------------------------------------------------
	bool Vector3::operator==(const Vector3& other) const
	{
		return x == other.x && y == other.y && z == other.z;
	}

	bool Vector3::operator!=(const Vector3& other) const
	{
		return !(*this == other);
	}

	/// -------------------------------------------------------------
	///				　　		二項演算子
	/// -------------------------------------------------------------
	Vector3 operator+(const Vector3& v1, const Vector3& v2) { return Vector3(v1) += v2; }

	Vector3 operator-(const Vector3& v1, const Vector3& v2) { return Vector3(v1) -= v2; }

	Vector3 operator*(const Vector3& v1, const Vector3& v2) { return Vector3(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z); }

	Vector3 operator*(const Vector3& v, float s) { return Vector3(v) *= s; }

	Vector3 operator*(float s, const Vector3& v) { return Vector3(v) *= s; }

	Vector3 operator/(const Vector3& v, float s) { return Vector3(v) /= s; }

	Vector3 operator*(const Matrix4x4& matrix, const Vector3& vec)
	{
		float x = matrix.m[0][0] * vec.x + matrix.m[1][0] * vec.y + matrix.m[2][0] * vec.z + matrix.m[3][0];
		float y = matrix.m[0][1] * vec.x + matrix.m[1][1] * vec.y + matrix.m[2][1] * vec.z + matrix.m[3][1];
		float z = matrix.m[0][2] * vec.x + matrix.m[1][2] * vec.y + matrix.m[2][2] * vec.z + matrix.m[3][2];

		return Vector3(x, y, z);
	}

} // namespace Ken4lowEngine
