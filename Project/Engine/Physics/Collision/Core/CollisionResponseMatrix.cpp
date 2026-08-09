#include "CollisionResponseMatrix.h"

namespace Ken4lowEngine
{
	CollisionResponseMatrix::CollisionResponseMatrix()
	{
		// 未設定のLayer同士は従来互換に近いBlockとして扱い、Debug側から必要なペアだけ変更する。
		for (auto& row : responses_)
		{
			row.fill(CollisionResponseType::Block);
		}
	}

	void CollisionResponseMatrix::SetResponse(uint32_t layerA, uint32_t layerB, CollisionResponseType response)
	{
		// 範囲外Layerは設定を無視し、Matrix内の既存設定を壊さない。
		if (layerA >= kMaxCollisionLayers || layerB >= kMaxCollisionLayers)
		{
			return;
		}

		responses_[layerA][layerB] = response;
		responses_[layerB][layerA] = response;
	}

	CollisionResponseType CollisionResponseMatrix::GetResponse(uint32_t layerA, uint32_t layerB) const
	{
		// 範囲外Layerは安全側としてIgnoreにし、意図しない接触生成を避ける。
		if (layerA >= kMaxCollisionLayers || layerB >= kMaxCollisionLayers)
		{
			return CollisionResponseType::Ignore;
		}

		return responses_[layerA][layerB];
	}

} // namespace Ken4lowEngine
