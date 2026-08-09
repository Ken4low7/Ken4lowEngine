#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Quaternion.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　	拡張ワールド変換クラス
	/// -------------------------------------------------------------
	class WorldTransformEx
	{
	public: /// ---------- メンバ変数 ---------- ///

		// オブジェクト単体の拡大率。
		Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

		// Euler 角で扱うローカル回転。
		Vector3 rotate_ = { 0.0f, 0.0f, 0.0f };

		// クォータニオン回転を使うか。
		// true の場合、Update() は rotate_ ではなく quaternion_ からローカル回転行列を作る。
		bool useQuaternionRotation_ = false;
		Quaternion quaternion_ = Quaternion::IdentityQuaternion();

		// 親を持つ場合は親から見た位置、親がない場合はワールド上の位置。
		Vector3 translate_ = { 0.0f, 0.0f, 0.0f };

		// Update() 後に確定するワールド座標。
		Vector3 worldTranslate_ = { 0.0f, 0.0f, 0.0f };

		// 親の回転を含めたワールド回転角。
		Vector3 worldRotate_ = { 0.0f, 0.0f, 0.0f };

		// Update() 後に確定するワールド変換行列。
		Matrix4x4 worldMatrix_;

		// 通常の親子付けを行う場合に参照する親のワールド変換。
		const WorldTransformEx* parent_ = nullptr;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 現在の scale / rotate または quaternion / translate からワールド行列を更新します。
		/// </summary>
		void Update();

		/// <summary>
		/// 親の向きとローカルオフセットを基に、追従用の位置と回転を更新します。
		/// </summary>
		/// <param name="parent">追従先となる親の WorldTransformEx。</param>
		/// <param name="offset">親から見たローカル位置オフセット。</param>
		/// <param name="preRotateX">オフセットへ事前に適用する X 軸回転角。</param>
		/// <param name="selfAdd">親の回転に追加する自身の回転補正値。</param>
		void Update(const WorldTransformEx* parent, const Vector3& offset, float preRotateX, const Vector3& selfAdd);
	};

} // namespace Ken4lowEngine
