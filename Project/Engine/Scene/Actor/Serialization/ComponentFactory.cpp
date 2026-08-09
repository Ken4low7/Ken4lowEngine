#include "ComponentFactory.h"

#include "Actor.h"
#include "AnimatedModelComponent.h"
#include "AudioComponent.h"
#include "BillboardComponent.h"
#include "CameraComponent.h"
#include "../Character/CharacterAnimationComponent.h"
#include "../Character/AttackComponent.h"
#include "../Character/CharacterColliderComponent.h"
#include "../Character/CharacterHealthComponent.h"
#include "../Character/CharacterMovementComponent.h"
#include "../Character/CharacterTargetComponent.h"
#include "../Character/HumanoidVisualComponent.h"
#include "ColliderComponent.h"
#include "StageColliderComponent.h"
#include "GaugeComponent.h"
#include "GpuParticleComponent.h"
#include "InstancedModelComponent.h"
#include "LightComponent.h"
#include "ModelComponent.h"
#include "RigidbodyComponent.h"
#include "SkeletalMeshComponent.h"
#include "SpriteComponent.h"
#include "TextComponent.h"
#include "WorldGaugeComponent.h"
#include "WorldAudioComponent.h"
#include "WorldSpriteComponent.h"
#include "WorldTextComponent.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		template<class T>
		ComponentFactory::ComponentTypeInfo MakeComponentTypeInfo(std::string className, bool allowMultiple, std::string displayName, std::string category, std::string description)
		{
			static_assert(std::is_base_of_v<ActorComponent, T>, "T must inherit from ActorComponent.");

			ComponentFactory::ComponentTypeInfo typeInfo{};
			typeInfo.className = std::move(className);
			typeInfo.displayName = std::move(displayName);
			typeInfo.category = std::move(category);
			typeInfo.description = std::move(description);
			typeInfo.allowMultiple = allowMultiple;
			typeInfo.canBeRoot = std::is_base_of_v<SceneComponent, T>;
			typeInfo.createFunc = [](Actor* owner) -> ActorComponent*
				{
					if (!owner)
					{
						return nullptr; // 所有Actorが無い場合はComponentを生成できない
					}

					return &owner->AddComponent<T>(); // ComponentをActorに追加する
				};
			typeInfo.createRootFunc = [](Actor* owner) -> SceneComponent*
				{
					if (!owner)
					{
						return nullptr; // 所有Actorが無い場合はComponentを生成できない
					}

					if constexpr (std::is_base_of_v<SceneComponent, T>)
					{
						return &owner->CreateRootComponent<T>(); // ComponentをRootComponentとして生成する
					}
					else
					{
						return nullptr; // SceneComponentではないComponentはRootにできない
					}
				};

			return typeInfo;
		}

		/// Engine組み込み型とApplicationLayerの実行時登録型を同じ一覧で管理する。
		std::vector<ComponentFactory::ComponentTypeInfo>& GetMutableRegisteredComponentTypes()
		{
			static std::vector<ComponentFactory::ComponentTypeInfo> registeredComponentTypes =
			{
			MakeComponentTypeInfo<SceneComponent>("SceneComponent", true, "シーンコンポーネント", "基本", "Actorに位置・回転・スケールと親子関係を持たせる基本Componentです。"),

			MakeComponentTypeInfo<CharacterHealthComponent>("CharacterHealthComponent", false, "キャラクターHP", "キャラクター", "HP計算、生存判定、無敵状態をCharacterActorから分離して管理します。"),

			MakeComponentTypeInfo<CharacterMovementComponent>("CharacterMovementComponent", false, "キャラクター移動", "キャラクター", "CharacterActorから移動計算を分離し、Root Transformへ速度を反映します。"),

			MakeComponentTypeInfo<CharacterTargetComponent>("CharacterTargetComponent", false, "キャラクターターゲット", "キャラクター", "攻撃、照準、追跡から参照するCharacterの代表位置を管理します。"),

			MakeComponentTypeInfo<CharacterColliderComponent>("CharacterColliderComponent", false, "キャラクター当たり判定", "キャラクター", "CharacterのTransformへ追従する共通の当たり判定を管理します。"),

			MakeComponentTypeInfo<CharacterAnimationComponent>("CharacterAnimationComponent", false, "キャラクターアニメーション", "キャラクター", "描画方式に依存しないアニメーション名、再生時間、ループ状態を管理します。"),

			MakeComponentTypeInfo<AttackComponent>("AttackComponent", false, "共通攻撃", "キャラクター", "攻撃データ、Cooldown、発動Phase、個別攻撃登録、攻撃イベントを共通管理します。"),

			// 既存Characterへ自動接続せず、EditorとJSONから選択して追加できる共通人型表示を登録する。
			MakeComponentTypeInfo<HumanoidVisualComponent>("HumanoidVisualComponent", false, "人型表示", "キャラクター", "人型部位の生成、親子Transform、表示、Material、JSONを共通管理します。"),

			MakeComponentTypeInfo<ModelComponent>("ModelComponent", true, "モデルコンポーネント", "描画", "Actorに3Dモデルの描画機能を追加します。"),

			MakeComponentTypeInfo<AnimatedModelComponent>("AnimatedModelComponent", true, "アニメーションモデルコンポーネント", "描画", "ボーンを使わない3Dモデルアニメーションを再生するComponentです。"),

			MakeComponentTypeInfo<InstancedModelComponent>("InstancedModelComponent", true, "インスタンスモデルコンポーネント", "描画", "同じモデルを複数まとめて描画するためのComponentです。"),

			MakeComponentTypeInfo<SkeletalMeshComponent>("SkeletalMeshComponent", true, "スケルタルメッシュコンポーネント", "描画", "Actorにボーン付き3Dモデルを表示し、アニメーションを再生するためのComponentです。"),

			MakeComponentTypeInfo<BillboardComponent>("BillboardComponent", true, "ビルボードコンポーネント", "描画", "3D空間上で常にカメラ方向を向く板ポリを表示するComponentです。"),

			MakeComponentTypeInfo<LightComponent>("LightComponent", true, "ライトコンポーネント", "描画", "Actorにライト情報を持たせるためのComponentです。"),

			MakeComponentTypeInfo<CameraComponent>("CameraComponent", false, "カメラコンポーネント", "カメラ", "Actorを視点として使うためのカメラ機能を追加します。"),

			MakeComponentTypeInfo<ColliderComponent>("ColliderComponent", true, "コライダーコンポーネント", "物理", "Actorに衝突判定用の形状と当たり判定設定を追加します。"),

			// Blender Level由来の用途名とTypeIdを失わず、通常ColliderComponentの物理経路を再利用する。
			MakeComponentTypeInfo<StageColliderComponent>("StageColliderComponent", true, "ステージコライダー", "物理", "Blender LevelのFloorやObstacleなどの識別情報を保持するステージ用Colliderです。"),

			MakeComponentTypeInfo<RigidbodyComponent>("RigidbodyComponent", false, "剛体コンポーネント", "物理", "Actorに速度や重力などの物理挙動を追加します。"),

			MakeComponentTypeInfo<SpriteComponent>("SpriteComponent", true, "スプライトコンポーネント", "UI", "画面上に2D画像を表示するためのComponentです。"),

			MakeComponentTypeInfo<WorldSpriteComponent>("WorldSpriteComponent", true, "ワールドスプライトコンポーネント", "UI", "Actorの3D位置を画面座標へ変換して画像を表示するComponentです。"),

			MakeComponentTypeInfo<TextComponent>("TextComponent", true, "テキストコンポーネント", "UI", "画面上に文字を表示するためのComponentです。"),

			MakeComponentTypeInfo<WorldTextComponent>("WorldTextComponent", true, "ワールドテキストコンポーネント", "UI", "Actorの3D位置を画面座標へ変換して文字を表示するComponentです。"),

			MakeComponentTypeInfo<GaugeComponent>("GaugeComponent", true, "ゲージコンポーネント", "UI", "画面上にHPバーや進行度バーを表示するためのComponentです。"),

			MakeComponentTypeInfo<WorldGaugeComponent>("WorldGaugeComponent", true, "ワールドゲージコンポーネント", "UI", "Actorの3D位置を画面座標へ変換してゲージを表示するComponentです。"),

			MakeComponentTypeInfo<AudioComponent>("AudioComponent", true, "オーディオコンポーネント", "オーディオ", "Actorに音声再生機能を持たせるためのComponentです。"),

			MakeComponentTypeInfo<WorldAudioComponent>("WorldAudioComponent", true, "ワールドオーディオコンポーネント", "オーディオ", "Actorの3D位置に基づいて距離減衰する音声を再生するComponentです。"),

			MakeComponentTypeInfo<GpuParticleComponent>("GpuParticleComponent", true, "GPUパーティクルコンポーネント", "演出", "ActorにGPUパーティクル演出を持たせるためのComponentです。"),
			};
			return registeredComponentTypes;
		}
	}

	ActorComponent* ComponentFactory::CreateComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return nullptr; // 未登録のComponentClass名の場合は生成しない
		}

		return typeInfo->createFunc(owner);
	}

	SceneComponent* ComponentFactory::CreateRootSceneComponent(Actor* owner, std::string_view className)
	{
		if (!owner)
		{
			return nullptr; // Actorがnullptrの場合はComponentを生成しない
		}

		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo || !typeInfo->canBeRoot)
		{
			return nullptr; // RootComponentとして使用できるSceneComponentか確認する
		}

		return typeInfo->createRootFunc(owner);
	}

	const std::vector<ComponentFactory::ComponentTypeInfo>& ComponentFactory::GetRegisteredComponentTypes()
	{
		// Add Component UIとFactory生成対象を同じ一覧に揃える
		return GetMutableRegisteredComponentTypes();
	}

	void ComponentFactory::RegisterComponentType(ComponentTypeInfo typeInfo)
	{
		if (typeInfo.className.empty() || !typeInfo.createFunc) return;
		auto& registeredTypes = GetMutableRegisteredComponentTypes();
		const auto existing = std::find_if(registeredTypes.begin(), registeredTypes.end(),
			[&typeInfo](const ComponentTypeInfo& candidate) { return candidate.className == typeInfo.className; });
		if (existing != registeredTypes.end())
		{
			*existing = std::move(typeInfo); // Scene再初期化時は同名登録を増やさず最新情報へ置き換える。
			return;
		}
		registeredTypes.push_back(std::move(typeInfo));
	}

	bool ComponentFactory::IsAllowMultiple(std::string_view className)
	{
		const ComponentTypeInfo* typeInfo = FindComponentType(className);

		if (!typeInfo)
		{
			return false; // 未登録のComponentClass名の場合は複数追加不可とする
		}

		return typeInfo->allowMultiple; // 同一Actorに複数追加可能かどうかを返す
	}

	const ComponentFactory::ComponentTypeInfo* ComponentFactory::FindComponentType(std::string_view className)
	{
		for (const ComponentFactory::ComponentTypeInfo& typeInfo : GetMutableRegisteredComponentTypes())
		{
			if (typeInfo.className == className)
			{
				return &typeInfo; // Class名が一致するComponent情報を返す
			}
		}

		return nullptr; // 一致するComponent情報がない場合はnullptrを返す
	}
} // namespace Ken4lowEngine
