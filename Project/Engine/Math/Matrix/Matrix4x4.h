#pragma once
#include <cmath>

namespace Ken4lowEngine
{

	class Vector3;
	class Quaternion;

	/// -------------------------------------------------------------
	///							4x4行列クラス
	/// -------------------------------------------------------------
	class Matrix4x4 final
	{
	public: /// ---------- メンバ変数 ---------- ///

		/// <summary>
		/// 行列に含まれる平行移動成分を取得します。
		/// 本クラスでは m[3][0], m[3][1], m[3][2] を平行移動 (x, y, z) として扱います。
		/// </summary>
		/// <returns>平行移動ベクトル</returns>
		Vector3 GetTranslation() const;

		/// <summary>
		/// 行列の各要素を保持する 4×4 の配列。
		/// m[row][column] 形式でアクセスします。
		/// </summary>
		float m[4][4];

	public:	/// ---------- コンストラクタ ---------- ///

		/// <summary>
		/// 全要素 0 で初期化された行列を生成します。
		/// </summary>
		Matrix4x4();

		/// <summary>
		/// 4×4 の float 配列から行列を初期化します。
		/// </summary>
		/// <param name="elements">[4][4] の 2 次元配列。m[i][j] に対応する値が格納されていることを前提とします。</param>
		Matrix4x4(float elements[4][4]);

		/// <summary>
		/// 各要素を個別に指定して行列を初期化します。
		/// m[row][column] の順で指定します。
		/// </summary>
		Matrix4x4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33) {
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
			m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
		}

	public: /// ---------- 演算子オーバーロード ---------- ///

		/// <summary>
		/// 行列の成分ごとの加算を行う複合代入演算子です。
		/// </summary>
		Matrix4x4& operator+=(const Matrix4x4& other);

		/// <summary>
		/// 行列の成分ごとの減算を行う複合代入演算子です。
		/// </summary>
		Matrix4x4& operator-=(const Matrix4x4& other);

		/// <summary>
		/// 行列同士の積（行列積）を計算して自身に代入します。
		/// 左から this、右から other を掛けた結果になります。
		/// </summary>
		Matrix4x4& operator*=(const Matrix4x4& other);

		/// <summary>
		/// 行列を別の行列で置き換えます。
		/// </summary>
		Matrix4x4& operator=(const Matrix4x4& other);

		/// <summary>
		/// 行列の成分ごとの加算を行います。
		/// </summary>
		Matrix4x4 operator+(const Matrix4x4& other) const { Matrix4x4 r = *this; r += other; return r; }

		/// <summary>
		/// 行列の成分ごとの減算を行います。
		/// </summary>
		Matrix4x4 operator-(const Matrix4x4& other) const { Matrix4x4 r = *this; r -= other; return r; }

		/// <summary>
		/// 行列同士の積（行列積）を計算します。
		/// 左から this、右から other を掛けた結果を返します。
		/// </summary>
		Matrix4x4 operator*(const Matrix4x4& other) const { Matrix4x4 r = *this; r *= other; return r; }

	public: /// ---------- 基本演算（静的関数） ---------- ///

		/// <summary>
		/// 2 つの行列の成分ごとの和を返します。
		/// operator+ の関数スタイル版です。
		/// </summary>
		static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

		/// <summary>
		/// 2 つの行列の成分ごとの差を返します。
		/// operator- の関数スタイル版です。
		/// </summary>
		static Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);

		/// <summary>
		/// 2 つの行列の行列積を返します。
		/// </summary>
		static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

		/// <summary>
		/// 行列の逆行列を計算して返します。
		/// 行列が正則（det ≠ 0）であることを前提とします。
		/// </summary>
		/// <param name="matrix">逆行列を求める対象の行列。</param>
		/// <returns>matrix の逆行列</returns>
		static Matrix4x4 Inverse(const Matrix4x4& matrix);

		/// <summary>
		/// 逆行列の計算を試みます。
		/// 逆行列が存在する場合は out に結果を格納して true を返します。
		/// 逆行列が存在しない場合は out を単位行列にして false を返します。
		/// </summary>
		static bool TryInverse(const Matrix4x4& matrix, Matrix4x4& out);

		/// <summary>
		/// 行列の転置行列を返します。
		/// </summary>
		/// <param name="m">転置する行列。</param>
		/// <returns>m の転置行列</returns>
		static Matrix4x4 Transpose(const Matrix4x4& m);

		/// <summary>
		/// 単位行列（対角成分が 1、それ以外が 0 の行列）を生成します。
		/// </summary>
		static Matrix4x4 MakeIdentity();

		/// <summary>
		/// ビュー行列（ルックアット行列）を作成します。
		/// </summary>
		/// <param name="eye">視点（カメラ）の位置。</param>
		/// <param name="target">視線の対象となる点。</param>
		/// <param name="up">上方向を示すベクトル。</param>
		/// <returns>ビュー変換を表す4x4行列。</returns>
		static Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);

		/// <summary>
		/// ライトのビュー・プロジェクション行列を作成します。
		/// </summary>
		/// <param name="lightDirection">ライトの方向ベクトル。</param>
		/// <param name="target">ライトが向くターゲット位置。</param>
		/// <param name="distanceFromTarget">ターゲットからの距離。</param>
		/// <param name="orthoHalfWidth">正射影の幅の半分。</param>
		/// <param name="orthoHalfHeight">正射影の高さの半分。</param>
		/// <param name="nearZ">ニアクリッピング面のZ座標。</param>
		/// <param name="farZ">ファークリッピング面のZ座標。</param>
		/// <returns>ライトのビュー・プロジェクション行列。</returns>
		static Matrix4x4 MakeLightViewProjection(const Vector3& lightDirection, const Vector3& target, float distanceFromTarget, float orthoHalfWidth, float orthoHalfHeight, float nearZ, float farZ);

	public: /// ---------- 変換行列の生成 ---------- ///

		/// <summary>
		/// スケール変換行列を生成します。
		/// </summary>
		/// <param name="scale">各軸方向のスケール値</param>
		static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

		/// <summary>
		/// X 軸まわりの回転行列を生成します。
		/// </summary>
		/// <param name="radian">回転角（ラジアン）</param>
		static Matrix4x4 MakeRotateX(float radian);

		/// <summary>
		/// Y 軸まわりの回転行列を生成します。
		/// </summary>
		/// <param name="radian">回転角（ラジアン）</param>
		static Matrix4x4 MakeRotateY(float radian);

		/// <summary>
		/// Z 軸まわりの回転行列を生成します。
		/// </summary>
		/// <param name="radian">回転角（ラジアン）</param>
		static Matrix4x4 MakeRotateZMatrix(float radian);

		/// <summary>
		/// XYZ の各軸回転を合成した回転行列を生成します。
		/// 引数のベクトルは (x, y, z) 軸まわりの回転角（ラジアン）を表します。
		/// 回転順序は X → Y → Z を想定しています。
		/// </summary>
		/// <param name="radian">各軸の回転角（ラジアン）</param>
		static Matrix4x4 MakeRotateMatrix(const Vector3& radian);

		/// <summary>
		/// 平行移動行列を生成します。
		/// </summary>
		/// <param name="translate">平行移動量 (x, y, z)。</param>
		static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

		/// <summary>
		/// スケール・回転（オイラー角）・平行移動を合成したアフィン変換行列を生成します。
		/// 生成される行列は Scale → Rotate(XYZ) → Translate の順で適用されます。
		/// </summary>
		static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

		/// <summary>
		/// スケール・回転（クォータニオン）・平行移動を合成したアフィン変換行列を生成します。
		/// </summary>
		static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	public: /// ---------- 投影・ビューポート ---------- ///

		/// <summary>
		/// 透視投影行列を生成します。
		/// DirectX のクリップ空間（右手/左手座標系）に対応した形式を想定しています。
		/// </summary>
		/// <param name="fovY">縦方向の画角（ラジアン）</param>
		/// <param name="aspectRatio">アスペクト比（横 / 縦）</param>
		/// <param name="nearClip">ニアクリップ面の距離</param>
		/// <param name="farClip">ファークリップ面の距離</param>
		static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

		/// <summary>
		/// 正射影行列を生成します。
		/// </summary>
		/// <param name="left">ビューボリュームの左端</param>
		/// <param name="top">ビューボリュームの上端</param>
		/// <param name="right">ビューボリュームの右端</param>
		/// <param name="bottom">ビューボリュームの下端</param>
		/// <param name="nearClip">ニアクリップ面の距離</param>
		/// <param name="farClip">ファークリップ面の距離</param>
		static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

		/// <summary>
		/// 正規化デバイス座標 (NDC) をスクリーン座標に変換するビューポート行列を生成します。
		/// </summary>
		/// <param name="left">ビューポートの左上 X 座標</param>
		/// <param name="top">ビューポートの左上 Y 座標</param>
		/// <param name="width">ビューポートの幅</param>
		/// <param name="height">ビューポートの高さ</param>
		/// <param name="minDepth">最小深度値</param>
		/// <param name="maxDepth">最大深度値</param>
		static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	public: /// ---------- その他ユーティリティ ---------- ///

		/// <summary>
		/// 任意の軸と角度から回転行列を生成します。
		/// </summary>
		/// <param name="axis">回転軸ベクトル。内部で正規化されます。</param>
		/// <param name="angle">回転角（ラジアン）</param>
		static Matrix4x4 MakeRotateAxisAngleMatrix(const Vector3& axis, float angle);

		/// <summary>
		/// アフィン行列をスケール・回転・平行移動成分に分解します。
		/// 回転は XYZ 順のオイラー角（ラジアン）として返します。
		/// </summary>
		/// <param name="matrix">分解するアフィン行列</param>
		/// <param name="outScale">スケール成分の出力先</param>
		/// <param name="outRotate">回転成分（オイラー角）の出力先</param>
		/// <param name="outTranslate">平行移動成分の出力先</param>
		static void Decompose(const Matrix4x4& matrix, Vector3& outScale, Vector3& outRotate, Vector3& outTranslate);

		/// <summary>
		/// 3D ベクトルを行列で座標変換します。
		/// (x, y, z, 1) をベクトルとして、matrix を掛けた結果の xyz を返します。
		/// </summary>
		/// <param name="vector">変換前のベクトル（ローカル空間など）</param>
		/// <param name="matrix">変換に使用する行列（ワールド・ビューなど）</param>
		/// <returns>変換後のベクトル。</returns>
		static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

		/// <summary>
		/// カメラ位置・注視点・上方向ベクトルからビュー行列（LookAt 行列）を生成します。
		/// </summary>
		/// <param name="eye">カメラの位置</param>
		/// <param name="target">カメラが注視する位置</param>
		/// <param name="up">カメラの上方向ベクトル</param>
		/// <returns>ビュー変換行列。</returns>
		static Matrix4x4 LookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
	};

} // namespace Ken4lowEngine
