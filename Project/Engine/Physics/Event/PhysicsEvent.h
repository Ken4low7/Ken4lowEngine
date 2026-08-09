#pragma once
#include "Contact.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         物理イベント種別
	/// -------------------------------------------------------------
	enum class PhysicsEventType
	{
		CollisionEnter,
		CollisionStay,
		CollisionExit,
		TriggerEnter,
		TriggerStay,
		TriggerExit,
	};

	/// -------------------------------------------------------------
	///                         物理イベント情報
	/// -------------------------------------------------------------
	struct PhysicsEvent
	{
		PhysicsEventType type = PhysicsEventType::CollisionEnter;
		Collider* colliderA = nullptr;
		Collider* colliderB = nullptr;
		Contact contact{};
		bool isTrigger = false;
	};

} // namespace Ken4lowEngine
