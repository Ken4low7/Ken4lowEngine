#pragma once
#include "LevelData.h"
#include <Object3D.h>
#include <AnimationModel.h>
#include "Collider.h"
#include "CollisionManager.h"

#include "AABB.h"

#include <memory>
#include <vector>

namespace Ken4lowEngine
{


	/// -------------------------------------------------------------
	///				レベルオブジェクトマネージャークラス
	/// -------------------------------------------------------------
	class LevelObjectManager : Collider
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// レベルデータをもとに、ステージオブジェクトやプレイヤーモデル、<br/>
		/// コライダーなどを生成します。<br/>
		/// ・最初に type == "MESH" のオブジェクトを 1 つだけステージモデルとして生成<br/>
		/// ・collider.enabled &amp;&amp; type == "BOX" のとき、スケールを反映した OBB コライダーを生成<br/>
		/// ・type == "PlayerSpawnPoint" のオブジェクトから AnimationModel を生成（スキニング有効）<br/>
		/// を行います。
		/// </summary>
		/// <param name="levelData">LevelLoader から渡されるレベルデータ。</param>
		/// <param name="modelName">
		/// ステージやプレイヤーモデルとして使用するモデル名（ファイル名）。<br/>
		/// Object3D / AnimationModel の Initialize に渡されます。
		/// </param>
		void Initialize(const LevelData& levelData, const std::string& defaultModelName);

		/// <summary>
		/// 管理している全てのオブジェクトを更新します。<br/>
		/// ・Object3D の Update()<br/>
		/// ・AnimationModel の Update()<br/>
		/// をそれぞれループして呼び出します。
		/// </summary>
		void Update();

		/// <summary>
		/// 管理している全てのオブジェクトを描画します。<br/>
		/// ・Object3D の Draw()<br/>
		/// ・AnimationModel の Draw()<br/>
		/// をそれぞれループして呼び出します。
		/// </summary>
		void Draw();

		/// <summary>
		/// 衝突時に呼ばれるコールバック関数です。<br/>
		/// 基底クラス Collider の仮想関数をオーバーライドしています。<br/>
		/// 現状は「相手がプレイヤーだった場合」の処理フックを用意しており、<br/>
		/// 実際の挙動は今後拡張していく想定です。
		/// </summary>
		/// <param name="other">衝突した相手のコライダー。</param>
		void OnCollision(Collider* other) override;

		/// <summary>
		/// 現在 LevelObjectManager が管理している、すべてのコライダー一覧を取得します。<br/>
		/// ColliderManager 等、外部の衝突判定システムに渡す用途を想定しています。
		/// </summary>
		/// <returns>Collider の unique_ptr を格納した配列の const 参照。</returns>
		const std::vector<std::unique_ptr<Collider>>& GetWorldColliders() const { return colliders_; }

		/// <summary>
		/// 管理している OBB コライダーを軸平行 AABB に変換した配列を取得します。<br/>
		/// 簡易なブロードフェーズ判定や、デバッグ描画用に AABB を扱いたい場合に利用します。
		/// </summary>
		/// <returns>各コライダーに対応する AABB の配列。</returns>
		std::vector<AABB> GetWorldAABBs() const {
			std::vector<AABB> result;
			result.reserve(colliders_.size());
			for (const auto& c : colliders_) {
				// 各コライダーの OBB を AABB に変換
				const OBB& obb = c->GetOBB();
				const Vector3& center = obb.center;
				const Vector3& half = obb.size; // すでに HalfSize 指定で Set 済み
				AABB box = {};
				box.min = center - half;
				box.max = center + half;
				result.push_back(box);
			}
			return result;
		}

	private: /// ---------- メンバ変数 ---------- ///

		LevelData levelData_ = {};  // レベルデータ

		// レベルデータから生成されたオブジェクトのリスト
		std::vector<std::unique_ptr<Object3D>> objects_;
		std::vector<std::unique_ptr<AnimationModel>> animationModels_; // アニメーションモデル用のリスト
		std::vector<std::unique_ptr<Collider>> colliders_; // コライダーのリスト

		std::unique_ptr<CollisionManager> collisionManager_; // 衝突マネージャー
	};

} // namespace Ken4lowEngine
