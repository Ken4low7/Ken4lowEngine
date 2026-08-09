#include "FrustumDebugRenderer.h"

#include "Wireframe.h"

namespace Ken4lowEngine
{
	/// ------------------------------------------------------------
	///          8点のコーナーを結ぶ線分を描画する処理
	/// ------------------------------------------------------------
	std::array<Vector3, 8> FrustumDebugRenderer::BuildCorners(const Matrix4x4& viewProjection) const
	{
		const Matrix4x4 inverseViewProjection = Matrix4x4::Inverse(viewProjection);
		const std::array<Vector3, 8> ndcCorners = {
			Vector3{ -1.0f, -1.0f, 0.0f }, // near 左下
			Vector3{ -1.0f, 1.0f, 0.0f },  // near 左上
			Vector3{ 1.0f, 1.0f, 0.0f },   // near 右上
			Vector3{ 1.0f, -1.0f, 0.0f },  // near 右下
			Vector3{ -1.0f, -1.0f, 1.0f }, // far 左下
			Vector3{ -1.0f, 1.0f, 1.0f },  // far 左上
			Vector3{ 1.0f, 1.0f, 1.0f },   // far 右上
			Vector3{ 1.0f, -1.0f, 1.0f },  // far 右下
		};

		// NDC 空間のコーナーを逆行列で変換してワールド空間のコーナーを求める
		std::array<Vector3, 8> worldCorners{};

		// NDC 空間のコーナーを逆行列で変換してワールド空間のコーナーを求める
		for (std::size_t i = 0; i < ndcCorners.size(); ++i)
		{
			worldCorners[i] = Vector3::Transform(ndcCorners[i], inverseViewProjection);
		}

		// ワールド空間のコーナーを返す
		return worldCorners;
	}

	/// ------------------------------------------------------------
	///							描画処理
	/// -------------------------------------------------------------
	void FrustumDebugRenderer::Draw(const Matrix4x4& viewProjection, const Vector4& color) const
	{
		Wireframe::GetInstance()->DrawFrustum(BuildCorners(viewProjection), color);
	}
}
