#pragma once

#include <functional>
#include <utility>

/// P11移行中だけStage1Tutorialの入力許可状態を新PlayerInputComponentへ渡すための一時ブリッジ。
class PlayerTutorialRestrictionBridge
{
public:
	struct State
	{
		bool enabled = false;
		bool allowMove = true;
		bool allowShoot = true;
		bool allowReload = true;
		bool allowWeaponSwitch = true;
	};

	using Provider = std::function<State()>;

	static void SetProvider(Provider provider) { provider_ = std::move(provider); }
	static void ClearProvider() { provider_ = {}; }
	static State GetState() { return provider_ ? provider_() : State{}; }

private:
	inline static Provider provider_{};
};
