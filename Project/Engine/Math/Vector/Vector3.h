#pragma once
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class Matrix4x4;

	/// -------------------------------------------------------------
	///							3次元ベクトル
	/// -------------------------------------------------------------
	class Vector3
	{
	public: /// ---------- メンバ変数 ---------- ///

		// 成分
		float x, y, z;

	public:	/// ---------- コンストラクタ ---------- ///

		/// <summary>
		/// 成分を 0 で初期化します。
		/// </summary>
		Vector3() : x(0), y(0), z(0) {};

		/// <summary>
		/// 指定した成分で初期化します。
		/// </summary>
		/// <param name="x">X 成分。</param>
		/// <param name="y">Y 成分。</param>
		/// <param name="z">Z 成分。</param>
		Vector3(float x, float y, float z) : x(x), y(y), z(z) {};

	public: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// 2 つのベクトルの成分ごとの和を返します。
		/// </summary>
		static Vector3 Add(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// 2 つのベクトルの成分ごとの差を返します。
		/// </summary>
		static Vector3 Subtract(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// ベクトルをスカラー倍した結果を返します。
		/// </summary>
		static Vector3 Multiply(float scalar, const Vector3& v);

		static Vector3 Multiply(const Vector3& v, float scalar) { return Multiply(scalar, v); }

		/// <summary>
		/// 2 つのベクトルの成分ごとの積を返します。
		/// </summary>
		static Vector3 Multiply(const Vector3& v1, const Vector3& v2) { return Vector3(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z); }

		static Vector3 PerpRightXZ(const Vector3& v) { return { v.z, 0.0f, -v.x }; }

		/// <summary>
		/// 2 つのベクトルの内積を返します。
		/// </summary>
		static float Dot(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// ベクトルの長さ（ノルム）を返します。
		/// </summary>
		static float Length(const Vector3& v);

		static float LengthXZ(const Vector3& v);

		static float LengthSquared(const Vector3& v);

		/// <summary>
		/// ベクトルを正規化します。
		/// 長さが 0 の場合は (0,0,0) を返します。
		/// </summary>
		static Vector3 Normalize(const Vector3& v);

		static Vector3 NormalizeXZ(const Vector3& v);

		static Vector3 NormalizeSafe(const Vector3& v, const Vector3& fallback = Vector3{ 0.0f, 0.0f, 0.0f });

		static Vector3 NormalizeXZSafe(const Vector3& v, const Vector3& fallback = { 0.0f, 0.0f, 1.0f })
		{
			const float len = LengthXZ(v);
			if (len <= 0.0001f) { return fallback; }
			return { v.x / len, 0.0f, v.z / len };
		}


		/// <summary>
		/// 4x4 行列を用いて 3D ベクトルを変換します。
		/// 内部的には w = 1 として同次座標に拡張し、w で割り戻します。
		/// </summary>
		static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

		/// <summary>
		/// 2 つのベクトルのクロス積を返します。
		/// </summary>
		static Vector3 Cross(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// Catmull-Rom スプライン補間を行います。
		/// P1〜P2 間の t の位置を返します。
		/// </summary>
		static Vector3 CatmullRomSpline(const Vector3& P0, const Vector3& P1, const Vector3& P2, const Vector3& P3, float t);

	public:	/// ---------- 演算子オーバーロード ---------- ///

		/// <summary>
		/// 単項プラス。自身をそのまま返します。
		/// </summary>
		Vector3 operator+() const;

		/// <summary>
		/// 単項マイナス。全成分の符号を反転します。
		/// </summary>
		Vector3 operator-() const;

		/// <summary>
		/// 成分ごとの加算を行う複合代入演算子です。
		/// </summary>
		Vector3& operator+=(const Vector3& other);

		/// <summary>
		/// 成分ごとの減算を行う複合代入演算子です。
		/// </summary>
		Vector3& operator-=(const Vector3& other);

		/// <summary>
		/// 全成分をスカラー倍する複合代入演算子です。
		/// </summary>
		Vector3& operator*=(float s);

		/// <summary>
		/// 全成分をスカラーで除算する複合代入演算子です。
		/// </summary>
		Vector3& operator/=(float s);

		/// <summary>
		/// 各成分が完全に一致しているかどうかで等価判定を行います。
		/// （浮動小数の誤差は考慮しません）
		/// </summary>
		bool operator==(const Vector3& other) const;
		bool operator!=(const Vector3& other) const;

		/// <summary>
		/// インデックスで要素にアクセスします（読み取り専用）。
		/// </summary>
		/// <param name="index">0:x、1:y、2:z。それ以外は std::out_of_range を送出します。</param>
		float operator[](int index) const
		{
			switch (index)
			{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			default: throw std::out_of_range("Index out of range");
			}
		}

		/// <summary>
		/// インデックスで要素にアクセスします（書き込み可能）。
		/// </summary>
		/// <param name="index">0:x、1:y、2:z。それ以外は std::out_of_range を送出します。</param>
		float& operator[](int index)
		{
			switch (index)
			{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			default: throw std::out_of_range("Index out of range");
			}
		}

	public:	/// ---------- 友達関数 ---------- ///

		/* Vector3 の加減算・積・除算・行列との積を
		   対称な二項演算子として扱うため、非メンバ + friend 関数として宣言しています。 */

		   /// <summary>
		   /// 2 つのベクトルの成分ごとの和を返します。
		   /// </summary>
		friend Vector3 operator+(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// 2 つのベクトルの成分ごとの差を返します。
		/// </summary>
		friend Vector3 operator-(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// 2 つのベクトルの成分ごとの積を返します。
		/// </summary>
		friend Vector3 operator*(const Vector3& v1, const Vector3& v2);

		/// <summary>
		/// ベクトルをスカラー倍した結果を返します。（ベクトル * スカラー）
		/// </summary>
		friend Vector3 operator*(const Vector3& v, float s);

		/// <summary>
		/// ベクトルをスカラー倍した結果を返します。（スカラー * ベクトル）
		/// </summary>
		friend Vector3 operator*(float s, const Vector3& v);

		/// <summary>
		/// ベクトルをスカラーで成分ごとに割った結果を返します。
		/// </summary>
		friend Vector3 operator/(const Vector3& v, float s);

		/// <summary>
		/// 行列とベクトルの積を計算します。
		/// 位置ベクトル (x,y,z,1) に Matrix4x4 を掛けた結果の xyz を返します。
		/// </summary>
		friend Vector3 operator*(const Matrix4x4& matrix, const Vector3& vec);
	};

} // namespace Ken4lowEngine
