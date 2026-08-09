#include "WorldTransformEx.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　			更新処理
	/// -------------------------------------------------------------
	void WorldTransformEx::Update()
	{
		// 回転方式に応じて、Euler 角またはクォータニオンからローカル変換行列を作成する。
		Matrix4x4 worldMatrix = useQuaternionRotation_
			? Matrix4x4::MakeAffineMatrix(scale_, quaternion_, translate_)
			: Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);

		// 親オブジェクトがあれば親のワールド行列を掛け、親子関係を反映した行列にする。
		if (parent_) worldMatrix = Matrix4x4::Multiply(worldMatrix, parent_->worldMatrix_);

		// 親の回転を引き継ぐ。
		// クォータニオン回転時も既存コード互換のため worldRotate_ は Euler の近似値として残す。
		worldRotate_ = parent_ ? parent_->worldRotate_ + rotate_ : rotate_;

		// 行列の平行移動成分から、外部参照用のワールド座標を更新する。
		worldTranslate_ = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };

		// 次の描画や子オブジェクトの計算で使えるよう、最終ワールド行列を保存する。
		worldMatrix_ = worldMatrix;
	}

	/// -------------------------------------------------------------
	///		拡張更新処理（親付き、オフセット・回転・加算あり）
	/// -------------------------------------------------------------
	void WorldTransformEx::Update(const WorldTransformEx* parent, const Vector3& offset, float preRotateX, const Vector3& selfAdd)
	{
		if (!parent) return; // 親がいない場合は追従先が決められないため何もしない。

		// 親のYaw/Pitchのみ継承（順序は既存コードに合わせて Rx→Ry）
		Matrix4x4 Rx = Matrix4x4::MakeRotateX(parent->rotate_.x);
		Matrix4x4 Ry = Matrix4x4::MakeRotateY(parent->rotate_.y);
		Matrix4x4 R = Matrix4x4::Multiply(Rx, Ry);

		// ローカルオフセットを 事前X回転 → 親のYaw/Pitch の順に変換して、ワールド空間上のずれ量にする。
		Matrix4x4 RxFix = Matrix4x4::MakeRotateX(preRotateX);
		Vector3 ofsLocalFixed = Matrix4x4::Transform(offset, RxFix);
		Vector3 ofsWorld = Matrix4x4::Transform(ofsLocalFixed, R);

		// 親の位置にワールド化したオフセットを足し、追従先の最終位置を決める。
		translate_ = parent->translate_ + ofsWorld;

		// 回転は親の回転を継承してから任意の微調整を加算
		rotate_ = parent->rotate_;
		rotate_.x += selfAdd.x;
		rotate_.y += selfAdd.y;
		rotate_.z += selfAdd.z;

		// この追従更新では Euler 角で回転を確定させるため、クォータニオン回転は無効にする。
		useQuaternionRotation_ = false;
	}

} // namespace Ken4lowEngine
