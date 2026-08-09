#include "DebugActorRegistration.h"

#include <ActorFactory.h>
#include <ComponentFactory.h>
#include <SceneComponent.h>

#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAIComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyAttackComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyEffectComponent.h"
#include "ApplicationLayer/Character/Enemy/Actor/MidRangeEnemyComponents.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "ApplicationLayer/Character/Boss/Actor/NonHumanoidBossActor.h"
#include "ApplicationLayer/Character/Boss/Actor/BossActorAttackComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossBrainComponent.h"
#include "ApplicationLayer/Character/Boss/Actor/BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossWeakPointComponent.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerActor.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerHudPresenterComponent.h"
#include "ApplicationLayer/Character/Player/Actor/PlayerMeleeAttackComponent.h"
#include "ApplicationLayer/Scene/StageSelectScene/StageSelectUIActor.h"
#include "Validation/PlayerMigrationValidationComponent.h"
#include "TestActor.h"
#include "TestGroundActor.h"

using namespace Ken4lowEngine;

namespace
{
	template<class T>
	ComponentFactory::ComponentTypeInfo MakeApplicationComponentTypeInfo(const char* className, const char* displayName, const char* category, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = category;
		typeInfo.description = description;
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = false;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent* { return owner ? &owner->AddComponent<T>() : nullptr; };
		return typeInfo;
	}

	template<class T>
	ComponentFactory::ComponentTypeInfo MakeApplicationSceneComponentTypeInfo(const char* className, const char* displayName, const char* category, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = category;
		typeInfo.description = description;
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = true;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent* { return owner ? &owner->AddComponent<T>() : nullptr; };
		typeInfo.createRootFunc = [](Actor* owner) -> SceneComponent* { return owner ? &owner->CreateRootComponent<T>() : nullptr; };
		return typeInfo;
	}
}

void RegisterApplicationActorTypes()
{
	static bool registered = false;
	if (registered) return;
	registered = true; // DebugSceneとGamePlaySceneの両方から呼ばれてもFactory登録を一度だけ行う。

	ActorFactory::RegisterActorClass<TestActor>("TestActor");
	ActorFactory::RegisterActorClass<TestGroundActor>("TestGroundActor");
	ActorFactory::RegisterActorClass<StageSelectUIActor>("StageSelectUIActor"); // ステージ選択文字UIもPrefab保存・再読込へ参加させる。
	ActorFactory::RegisterActorClass<EnemyActor>("EnemyActor");
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyAIComponent>("EnemyAIComponent", "通常敵AI", "通常敵", "A*追跡判断と移動速度出力を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyAttackComponent>("EnemyAttackComponent", "通常敵攻撃", "通常敵", "ScratchとLungeScratchを共通攻撃基盤で管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<MidRangeEnemyAIComponent>("MidRangeEnemyAIComponent", "中距離敵AI", "通常敵", "間合い維持、後退、A*接近、徘徊を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<MidRangeEnemyAttackComponent>("MidRangeEnemyAttackComponent", "中距離敵攻撃", "通常敵", "爆弾投擲、Projectile、自爆モードを管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<EnemyEffectComponent>("EnemyEffectComponent", "通常敵Effect", "通常敵", "被弾・死亡Effectと死亡表示を管理します。"));

	ActorFactory::RegisterActorClass<BossActor>("BossActor");
	ActorFactory::RegisterActorClass<NonHumanoidBossActor>("NonHumanoidBossActor");
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossBrainComponent>("BossBrainComponent", "ボス行動判断", "ボス", "Target追跡と攻撃要求を判断します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<Ken4lowEngine::BossAttackComponent>("BossAttackComponent", "ボス攻撃", "ボス", "フェーズ別攻撃選択と共通攻撃実行を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossPhaseComponent>("BossPhaseComponent", "ボスフェーズ", "ボス", "共通HPからフェーズを判定します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossWeakPointComponent>("BossWeakPointComponent", "ボス弱点", "ボス", "人型部位IDを参照して弱点倍率を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<BossPresentationComponent>("BossPresentationComponent", "ボス演出", "ボス", "フェーズ遷移と死亡演出を管理します。"));

	ActorFactory::RegisterActorClass<PlayerActor>("PlayerActor");
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerInputComponent>("PlayerInputComponent", "プレイヤー入力", "プレイヤー", "入力を各専用Componentへ配送します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerMovementComponent>("PlayerMovementComponent", "プレイヤー移動", "プレイヤー", "通常移動・Ladder・落下ダメージ・被弾ノックバックを管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<WeaponComponent>("WeaponComponent", "武器", "プレイヤー", "射撃・リロード・装備演出と弾薬状態を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<InventoryComponent>("InventoryComponent", "Inventory", "プレイヤー", "武器スロットと選択中の装備を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerMeleeAttackComponent>("PlayerMeleeAttackComponent", "プレイヤー近接攻撃", "プレイヤー", "近接攻撃の予備・有効・硬直と命中判定を管理します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerHudPresenterComponent>("PlayerHudPresenterComponent", "プレイヤーHUD", "プレイヤー", "HP・武器・NoAmmo・HitMarker・Crosshairを同期します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationComponentTypeInfo<PlayerMigrationValidationComponent>("PlayerMigrationValidationComponent", "Player検証", "デバッグ", "新PlayerのDamage・Heal・Fire・Reload・Death・Resetを検証します。"));
	ComponentFactory::RegisterComponentType(MakeApplicationSceneComponentTypeInfo<PlayerCameraComponent>("PlayerCameraComponent", "プレイヤーカメラ", "プレイヤー", "視点要求を共通CameraComponentへ同期します。"));
}

void RegisterDebugActors(){ RegisterApplicationActorTypes(); }
