#include "LevelObjectManager.h"
#include "CollisionTypeIdDef.h"

namespace Ken4lowEngine
{


	/// -------------------------------------------------------------
	///				　			　 初期化処理
	/// -------------------------------------------------------------
	void LevelObjectManager::Initialize(const LevelData& levelData, const std::string& defaultModelName)
	{
		objects_.clear();
		colliders_.clear();

		// ステージ生成フラグ
		bool stageCreated = false;

		// 衝突マネージャーの初期化
		if (!collisionManager_)
		{
			collisionManager_ = std::make_unique<CollisionManager>();
			collisionManager_->Initialize();
		}

		// レベルデータのオブジェクトをループして生成
		for (const ObjectData& data : levelData.objects)
		{
			//if (data.type == "MESH") continue;

			if (!stageCreated)
			{
				const std::string& modelToLoad =
					(!data.modelName.empty()) ? data.modelName : defaultModelName;

				std::unique_ptr<Object3D> obj = std::make_unique<Object3D>();
				obj->Initialize(modelToLoad);
				obj->SetTranslate(data.position);
				obj->SetRotate(data.rotation);
				obj->SetScale(data.scale);
				objects_.push_back(std::move(obj));
				stageCreated = true; // ステージ生成済みフラグを立てる
			}

			// コライダーの生成
			if (data.collider.enabled && data.collider.type == "BOX")
			{
				// ローカル→ワールド（スケール反映）
				const Vector3 centerW = {
					data.position.x + data.collider.center.x * data.scale.x,
					data.position.y + data.collider.center.y * data.scale.y,
					data.position.z + data.collider.center.z * data.scale.z,
				};
				// size(フルサイズ) → half(半サイズ)
				const Vector3 halfW = {
					0.5f * data.collider.size.x * data.scale.x,
					0.5f * data.collider.size.y * data.scale.y,
					0.5f * data.collider.size.z * data.scale.z,
				};

				auto up = std::make_unique<Collider>();
				Collider* raw = up.get(); // 先に生ポインタを保持
				raw->SetCenterPosition(centerW);
				raw->SetOBBHalfSize(halfW); // Half-Extentsを渡す

				// 先に vector に move してから、保持した raw を登録
				colliders_.push_back(std::move(up));
			}
		}
	}


	/// -------------------------------------------------------------
	///				　			　 更新処理
	/// -------------------------------------------------------------
	void LevelObjectManager::Update()
	{
		// オブジェクトの更新
		for (auto& obj : objects_)
		{
			obj->Update();
		}

		// アニメーションモデルの更新を追加
		for (auto& anim : animationModels_)
		{
			anim->Update();
		}
	}


	/// -------------------------------------------------------------
	///				　		    描画処理
	/// -------------------------------------------------------------
	void LevelObjectManager::Draw()
	{
		// オブジェクトの描画
		for (auto& obj : objects_)
		{
			obj->Draw();
		}

		// アニメーションモデルの描画を追加
		for (auto& anim : animationModels_)
		{
			anim->Draw();
		}
	}

	/// -------------------------------------------------------------
	///				　	衝突時に呼ばれる仮想関数
	/// -------------------------------------------------------------
	void LevelObjectManager::OnCollision(Collider* other)
	{
		if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer))
		{
			// プレイヤーが敵と衝突した場合の処理をここに記述
		}
	}

} // namespace Ken4lowEngine
