#pragma once
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							4次元ベクトル
	/// -------------------------------------------------------------
	class Vector4
	{
	public: /// ---------- メンバ変数 ---------- ///

		float x, y, z, w;

	public: /// ---------- コンストラクタ ---------- ///

		Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
		Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	public: /// ---------- 演算子オーバーロード ---------- ///

		Vector4 operator+() const { return *this; }
		Vector4 operator-() const { return { -x, -y, -z, -w }; }

		Vector4 operator+(const Vector4& other) const { return { x + other.x, y + other.y, z + other.z, w + other.w }; }

		Vector4 operator-(const Vector4& other) const { return { x - other.x, y - other.y, z - other.z, w - other.w }; }

		Vector4 operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar, w * scalar }; }

		Vector4 operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar, w / scalar }; }

		Vector4& operator+=(const Vector4& other)
		{
			x += other.x;
			y += other.y;
			z += other.z;
			w += other.w;
			return *this;
		}

		Vector4& operator-=(const Vector4& other)
		{
			x -= other.x;
			y -= other.y;
			z -= other.z;
			w -= other.w;
			return *this;
		}

		Vector4& operator*=(float scalar)
		{
			x *= scalar;
			y *= scalar;
			z *= scalar;
			w *= scalar;
			return *this;
		}

		Vector4& operator/=(float scalar)
		{
			x /= scalar;
			y /= scalar;
			z /= scalar;
			w /= scalar;
			return *this;
		}

		bool operator==(const Vector4& other) const { return x == other.x && y == other.y && z == other.z && w == other.w; }

		bool operator!=(const Vector4& other) const { return !(*this == other); }

		float operator[](int index) const
		{
			switch (index)
			{
			case 0:
			return x;
			case 1:
			return y;
			case 2:
			return z;
			case 3:
			return w;
			default:
			throw std::out_of_range("Vector4 index out of range");
			}
		}

		float& operator[](int index)
		{
			switch (index)
			{
			case 0:
			return x;
			case 1:
			return y;
			case 2:
			return z;
			case 3:
			return w;
			default:
			throw std::out_of_range("Vector4 index out of range");
			}
		}

	public: /// ---------- 静的メンバ関数 ---------- ///

		static float LengthSquared(const Vector4& v) { return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w; }
	};

	inline Vector4 operator*(float scalar, const Vector4& v) { return v * scalar; }

} // namespace Ken4lowEngine