#pragma once

#include <Vector3.h>

namespace Ken4lowEngine
{
	class Camera;
	class Collider;
}

/// GamePlay側が巨大な具象Playerへ直接依存せず、PlayerActorを正本として扱うRuntime境界。
class IPlayerRuntime
{
public:
	IPlayerRuntime() { activeRuntime_ = this; }
	virtual ~IPlayerRuntime()
	{
		if (activeRuntime_ == this) activeRuntime_ = nullptr; // RetryやScene破棄後に古いPlayer参照を残さない。
	}

	virtual float GetHP() const = 0;
	virtual float GetMaxHP() const = 0;
	virtual bool IsGameOverReady() const = 0;
	virtual bool ConsumeGameOverReady() = 0;
	virtual bool IsDeathActive() const = 0;
	bool IsDeathSequenceFinished() const { return IsGameOverReady(); }

	// 単一PlayerのGamePlay HUDが新Runtimeへ直接接続できるよう、現在有効なRuntimeを登録する。
	static void SetActiveRuntime(IPlayerRuntime* runtime) { activeRuntime_ = runtime; }
	static IPlayerRuntime* GetActiveRuntime() { return activeRuntime_; }
	static const IPlayerRuntime* GetActiveRuntimeConst() { return activeRuntime_; }

	// P13中も旧Playerソース自体を比較用にコンパイルできるよう、追加Runtime APIは安全な既定実装を持つ。
	virtual Ken4lowEngine::Vector3 GetWorldPosition() const { return {}; }
	virtual Ken4lowEngine::Collider* GetCollisionPrimitive() { return nullptr; }
	virtual const Ken4lowEngine::Collider* GetCollisionPrimitive() const { return nullptr; }
	Ken4lowEngine::Camera* GetCamera()
	{
		// 非const呼び出しもconst仮想関数へ集約し、旧PlayerとPlayerActorの既存Camera APIを共存させる。
		return const_cast<Ken4lowEngine::Camera*>(static_cast<const IPlayerRuntime*>(this)->GetCamera());
	}
	virtual const Ken4lowEngine::Camera* GetCamera() const { return nullptr; }

	virtual float ApplyRuntimeDamage(float amount)
	{
		(void)amount;
		return 0.0f;
	}
	virtual float HealRuntime(float amount)
	{
		(void)amount;
		return 0.0f;
	}
	virtual void ApplyDamage(float amount, const Ken4lowEngine::Vector3* attackPosition = nullptr)
	{
		(void)attackPosition;
		ApplyRuntimeDamage(amount); // 旧Boss攻撃の位置付きDamage呼び出しをRuntime境界へ吸収する。
	}

	virtual int AddReserveAmmo(int amount)
	{
		(void)amount;
		return 0;
	}
	virtual int GetMagazineAmmo() const { return 0; }
	virtual int GetMagazineCapacity() const { return 0; }
	virtual int GetReserveAmmo() const { return 0; }
	virtual int GetMaxReserveAmmo() const { return 0; }
	virtual bool IsReloading() const { return false; }
	virtual float GetReloadTimer() const { return 0.0f; }
	virtual float GetReloadDuration() const { return 0.0f; }
	virtual int GetSelectedWeaponSlot() const { return 0; }
	virtual int GetWeaponSlotCount() const { return 1; }
	virtual int GetWeaponIdForSlot(int slotIndex) const { return slotIndex == 0 ? 0 : -1; }

	virtual void SetViewLookAngles(float pitch, float yaw)
	{
		(void)pitch;
		(void)yaw;
	}

private:
	inline static IPlayerRuntime* activeRuntime_ = nullptr;
};