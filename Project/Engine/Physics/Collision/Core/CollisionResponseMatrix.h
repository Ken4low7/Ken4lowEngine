#pragma once
#include "PhysicsTypes.h"
#include "PhysicsCollisionLayer.h"

#include <array>
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                   衝突応答マトリクス
	/// -------------------------------------------------------------
	class CollisionResponseMatrix
	{
	public: /// ---------- 定数 ---------- ///

		static constexpr uint32_t kMaxCollisionLayers = 32u;

	public: /// ---------- メンバ関数 ---------- ///

		// Layer同士の衝突応答を管理するため、初期状態はすべてBlockにする。
		CollisionResponseMatrix();

		// 指定Layerペアの応答を設定する。A->BとB->Aを同じ値に保つ。
		void SetResponse(uint32_t layerA, uint32_t layerB, CollisionResponseType response);

		/// <summary>
		/// 名前付きLayerペアの応答を設定する
		/// </summary>
		void SetResponse(PhysicsCollisionLayer layerA, PhysicsCollisionLayer layerB, CollisionResponseType response)
		{
			SetResponse(static_cast<uint32_t>(layerA), static_cast<uint32_t>(layerB), response);
		}

		// 指定Layerペアの応答を取得する。範囲外LayerはIgnoreとして扱う。
		CollisionResponseType GetResponse(uint32_t layerA, uint32_t layerB) const;

	private: /// ---------- メンバ変数 ---------- ///

		// Layer同士のIgnore/Trigger/Block設定。
		std::array<std::array<CollisionResponseType, kMaxCollisionLayers>, kMaxCollisionLayers> responses_{};
	};

} // namespace Ken4lowEngine
