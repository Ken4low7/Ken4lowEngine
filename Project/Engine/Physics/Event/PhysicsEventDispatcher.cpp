#include "PhysicsEventDispatcher.h"

#include <algorithm>

namespace Ken4lowEngine
{
	ContactPairKey ContactPairKey::Make(Collider* colliderA, Collider* colliderB)
	{
		// nullptrが混ざるContactはイベント対象外にするため、無効キーとして返す。
		if (!colliderA || !colliderB)
		{
			return {};
		}

		const uintptr_t addressA = reinterpret_cast<uintptr_t>(colliderA);
		const uintptr_t addressB = reinterpret_cast<uintptr_t>(colliderB);
		return addressA <= addressB ? ContactPairKey{ colliderA, colliderB } : ContactPairKey{ colliderB, colliderA };
	}

	size_t ContactPairKeyHash::operator()(const ContactPairKey& key) const
	{
		// 正規化済みの2ポインタを混ぜ、unordered_mapでContactペアを扱えるようにする。
		const size_t hashA = std::hash<Collider*>{}(key.a);
		const size_t hashB = std::hash<Collider*>{}(key.b);
		return hashA ^ (hashB + 0x9e3779b9u + (hashA << 6) + (hashA >> 2));
	}

	void PhysicsEventDispatcher::Update(const std::vector<Contact>& contacts)
	{
		// 前フレームと現在フレームのContactを比較してEnter/Stay/Exitを生成する。
		events_.clear();
		currentContacts_.clear();

		for (const Contact& contact : contacts)
		{
			const ContactPairKey key = ContactPairKey::Make(contact.colliderA, contact.colliderB);
			if (!key.a || !key.b)
			{
				continue;
			}

			currentContacts_[key] = contact;
		}

		for (const auto& [key, currentContact] : currentContacts_)
		{
			const auto previousIt = previousContacts_.find(key);
			const bool isTrigger = currentContact.isTrigger;
			if (previousIt == previousContacts_.end())
			{
				PushEvent(BuildEvent(isTrigger ? PhysicsEventType::TriggerEnter : PhysicsEventType::CollisionEnter, currentContact));
				continue;
			}

			if (previousIt->second.isTrigger != currentContact.isTrigger)
			{
				// Trigger/Blockの種別が変わった場合は、古い接触の終了と新しい接触の開始として扱う。
				PushEvent(BuildEvent(previousIt->second.isTrigger ? PhysicsEventType::TriggerExit : PhysicsEventType::CollisionExit, previousIt->second));
				PushEvent(BuildEvent(isTrigger ? PhysicsEventType::TriggerEnter : PhysicsEventType::CollisionEnter, currentContact));
				continue;
			}

			PushEvent(BuildEvent(isTrigger ? PhysicsEventType::TriggerStay : PhysicsEventType::CollisionStay, currentContact));
		}

		for (const auto& [key, previousContact] : previousContacts_)
		{
			if (currentContacts_.find(key) != currentContacts_.end())
			{
				continue;
			}

			const bool wasTrigger = previousContact.isTrigger;
			PushEvent(BuildEvent(wasTrigger ? PhysicsEventType::TriggerExit : PhysicsEventType::CollisionExit, previousContact));
		}

		previousContacts_ = currentContacts_;
	}

	void PhysicsEventDispatcher::Clear()
	{
		// Contact履歴だけをリセットし、登録済みListenerは維持する。
		previousContacts_.clear();
		currentContacts_.clear();
		events_.clear();
	}

	void PhysicsEventDispatcher::AddListener(IPhysicsEventListener* listener)
	{
		// nullptrと重複登録を避け、外部所有のListener参照だけを保持する。
		if (!listener || std::find(listeners_.begin(), listeners_.end(), listener) != listeners_.end())
		{
			return;
		}

		listeners_.push_back(listener);
	}

	void PhysicsEventDispatcher::RemoveListener(IPhysicsEventListener* listener)
	{
		// Listenerの所有権は持たないため、通知リストからのみ取り除く。
		listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
	}

	PhysicsEvent PhysicsEventDispatcher::BuildEvent(PhysicsEventType type, const Contact& contact) const
	{
		// Exitでも最低限のColliderペアが確認できるよう、Contactをそのままイベントへ保持する。
		PhysicsEvent event{};
		event.type = type;
		event.colliderA = contact.colliderA;
		event.colliderB = contact.colliderB;
		event.contact = contact;
		event.isTrigger = contact.isTrigger;
		return event;
	}

	void PhysicsEventDispatcher::PushEvent(const PhysicsEvent& event)
	{
		// イベントログとListener通知を同じタイミングで行い、Debug表示と外部購読を同期させる。
		events_.push_back(event);
		for (IPhysicsEventListener* listener : listeners_)
		{
			if (listener)
			{
				listener->OnPhysicsEvent(event);
			}
		}
	}

} // namespace Ken4lowEngine
