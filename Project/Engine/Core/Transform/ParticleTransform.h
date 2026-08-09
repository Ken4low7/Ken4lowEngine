#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

namespace Ken4lowEngine
{
	
	/// -------------------------------------------------------------
	///				パーティクル用の座標変換データクラス
	/// -------------------------------------------------------------
	class ParticleTransform
	{
	public: /// ---------- メンバ変数 ---------- ///

		// パーティクルのローカルスケール。
		Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

		// パーティクルのローカル回転角。ビルボード時は主に Z 回転だけを使用する。
		Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

		// パーティクルのワールド配置位置。
		Vector3 translate_ = { 0.0f, 0.0f, 0.0f };

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// パーティクルの World 行列と WVP 行列を更新します。
		/// </summary>
		/// <param name="viewProjection">カメラの ViewProjection 行列。</param>
		/// <param name="useBillboard">true の場合、カメラ方向を向くビルボード行列を使用します。</param>
		/// <param name="billboardMatrix">ビルボード用の回転行列。useBillboard が false の場合は使用しません。</param>
		void UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix);

		/// <summary>
		/// オブジェクトのワールド変換行列（Matrix4x4）への const 参照を返します。メソッドは const であり、オブジェクトの状態を変更しません。
		/// </summary>
		/// <returns>worldMatrix_ を指す const Matrix4x4 への参照。呼び出し側で行列を変更しないことを意図した参照です。</returns>
		const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

		/// <summary>
		/// WVP（ワールド・ビュー・プロジェクション）行列への定数参照を返します。
		/// </summary>
		/// <returns>Matrix4x4 型で表される WVP 行列への const 参照。</returns>
		const Matrix4x4& GetWVPMatrix() const { return wvpMatrix_; }

	private: /// ---------- メンバ変数 ---------- ///

		// ワールド変換行列
		Matrix4x4 worldMatrix_ = Matrix4x4::MakeIdentity();

		// WVP行列
		Matrix4x4 wvpMatrix_ = Matrix4x4::MakeIdentity();
	};

} // namespace Ken4lowEngine
