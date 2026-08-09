#pragma once
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///							2次元ベクトル
	/// -------------------------------------------------------------
	class Vector2 final
	{
	public: /// ---------- メンバ変数 ---------- ///

		// 成分
		float x, y;

	public:	/// ---------- 二項演算子 ---------- ///

		/// <summary>
		/// 2 つのベクトルの成分ごとの和・差・積・商を求めます。
		/// 例： (x, y) + (x', y') = (x + x', y + y')
		/// </summary>
		Vector2 operator+(const Vector2& other) const { return { x + other.x, y + other.y }; };
		Vector2 operator-(const Vector2& other) const { return { x - other.x, y - other.y }; };
		Vector2 operator*(const Vector2& other) const { return { x * other.x, y * other.y }; };
		Vector2 operator/(const Vector2& other) const { return { x / other.x, y / other.y }; };

	public:	/// ---------- スカラー演算子 ---------- ///

		/// <summary>
		/// Vector2 をスカラー値で乗算し、その結果の新しい Vector2 を返します。
		/// </summary>
		/// <param name="scalar">各成分に乗算する浮動小数点スカラー値。</param>
		/// <returns>スカラー倍された新しい Vector2。元のベクトルは変更されません。</returns>
		Vector2 operator*(float scalar) const { return { x * scalar, y * scalar }; }

		/// <summary>
		/// このメンバ演算子は、2 次元ベクトルの各成分を指定したスカラーで割った新しい Vector2 を返します。
		/// </summary>
		/// <param name="scalar">各成分を割るスカラー値。0 を指定すると結果が無限大または NaN になる可能性があります。</param>
		/// <returns>スカラーで各成分を割った新しい Vector2。</returns>
		Vector2 operator/(float scalar) const { return { x / scalar, y / scalar }; }

	public:	/// ---------- 複合代入演算子（参照返し） ---------- ///

		/// <summary>
		/// 複合代入演算子。各成分を加算・減算・スカラー倍・スカラー除算します。
		/// </summary>
		Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; }
		Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }
		Vector2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
		Vector2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

	public: /// ---------- 等価演算子 ---------- ///

		/// <summary>
		/// 各成分が完全に一致しているかどうかで等価判定を行います。（誤差は考慮しません）
		/// </summary>
		bool operator==(const Vector2& other) const { return x == other.x && y == other.y; }
		bool operator!=(const Vector2& other) const { return !(*this == other); }

	public: /// ---------- 単項演算子 ---------- ///

		/// <summary>
		/// 単項プラス／マイナス演算子。符号反転のみ行います。
		/// </summary>
		Vector2 operator+() const { return *this; }
		Vector2 operator-() const { return { -x, -y }; }

		/// <summary>
		/// インデックスで要素にアクセスします（読み取り専用）。
		/// </summary>
		/// <param name="index">0 なら x、1 なら y。それ以外は未定義動作です。</param>
		float operator[](int index) const
		{
			switch (index)
			{
			case 0:
			return x;
			case 1:
			return y;
			default:
			throw std::out_of_range("Vector2 index out of range");
			}
		}

		/// <summary>
		/// インデックスで要素にアクセスします（書き込み可能）。
		/// </summary>
		/// <param name="index">0 なら x、1 なら y。それ以外は未定義動作です。</param>
		float& operator[](int index)
		{
			switch (index)
			{
			case 0:
			return x;
			case 1:
			return y;
			default:
			throw std::out_of_range("Vector2 index out of range");
			}
		}

	public: /// ---------- 静的メンバ関数 ---------- ///

		static float LengthSquared(const Vector2& v) { return v.x * v.x + v.y * v.y; }
	};

	/// <summary>
	/// ベクトルのスカラー倍を行います（スカラーが左側）。
	/// v * scalar と同じ結果を返します。
	/// </summary>
	/// <param name="scalar">スカラー値。</param>
	/// <param name="v">スカラー倍されるベクトル。</param>
	/// <returns>各成分を scalar 倍したベクトル。</returns>
	inline Vector2 operator*(float scalar, const Vector2& v) { return v * scalar; }
} // namespace Ken4lowEngine
