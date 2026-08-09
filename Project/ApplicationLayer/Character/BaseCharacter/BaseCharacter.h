#pragma once

#include <Scene/Actor/Character/HumanoidCharacterActor.h>

// 旧ApplicationLayerコードがBaseCharacter.h経由で利用していた名前空間エイリアスを維持する。
namespace K4E = ::Ken4lowEngine;

/// 旧includeパスを壊さず、実体は共通Actor/Component構成へ一本化する。
using BaseCharacter = Ken4lowEngine::HumanoidCharacterActor;
