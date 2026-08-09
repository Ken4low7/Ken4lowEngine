#pragma once
#include "PhysicsEvent.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         物理イベントリスナー
	/// -------------------------------------------------------------
	class IPhysicsEventListener
	{
	public:
		virtual ~IPhysicsEventListener() = default;

		// PhysicsWorldで生成された物理イベントを受け取る。
		virtual void OnPhysicsEvent(const PhysicsEvent& event) = 0;
	};

} // namespace Ken4lowEngine
