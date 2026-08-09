#include "ParticleTransform.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				パーティクル用の行列更新処理
	/// -------------------------------------------------------------
	void ParticleTransform::UpdateMatrix(const Matrix4x4& viewProjection, bool useBillboard, const Matrix4x4& billboardMatrix)
	{
		// 描画方式に応じて、通常のアフィン変換かビルボード変換のどちらかで World 行列を構築する。
		if (useBillboard) // ビルボード変換を使用する場合
		{
			// ビルボードはカメラ方向を向くため、個別回転は見た目調整用の Z 軸回転だけを残す。
			Matrix4x4 rotZMat = Matrix4x4::MakeRotateZMatrix(rotate_.z);

			// パーティクルごとの大きさを反映する。
			Matrix4x4 scaleMat = Matrix4x4::MakeScaleMatrix(scale_);

			// カメラ正面を向くための基底行列を受け取って使用する。
			Matrix4x4 facingMat = billboardMatrix;

			// カメラ向きの回転にローカル Z 回転を合成し、板ポリの傾きだけを個別に変えられるようにする。
			Matrix4x4 combinedRot = Matrix4x4::Multiply(rotZMat, facingMat);

			// パーティクルをワールド上の発生位置へ移動する。
			Matrix4x4 transMat = Matrix4x4::MakeTranslateMatrix(translate_);

			// 合成順は scale → rotation → translation。順番を変えると拡大や回転の基準がずれる。
			worldMatrix_ = Matrix4x4::Multiply(Matrix4x4::Multiply(scaleMat, combinedRot), transMat);
		}
		else // 通常の変換
		{
			// メッシュ風に XYZ 回転をそのまま使う通常のアフィン変換行列を作成する。
			worldMatrix_ = Matrix4x4::MakeAffineMatrix(scale_, rotate_, translate_);
		}

		// シェーダーへ渡すため、World と ViewProjection を合成した WVP 行列を更新する。
		wvpMatrix_ = Matrix4x4::Multiply(worldMatrix_, viewProjection);
	}

} // namespace Ken4lowEngine
