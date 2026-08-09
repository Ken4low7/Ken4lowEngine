#pragma once
#include "Contact.h"
#include "IPhysicsEventListener.h"
#include "PhysicsEvent.h"

#include <cstdint>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         Contactペアキー
	/// -------------------------------------------------------------
	struct ContactPairKey
	{
		Collider* a = nullptr;
		Collider* b = nullptr;

		// Colliderの順番違いで別ペアにならないように、アドレス順へ正規化してキーを作る。
		static ContactPairKey Make(Collider* colliderA, Collider* colliderB);

		bool operator==(const ContactPairKey& other) const
		{
			return a == other.a && b == other.b;
		}
	};

	/// -------------------------------------------------------------
	///                         Contactペアキーハッシュ
	/// -------------------------------------------------------------
	struct ContactPairKeyHash
	{
		size_t operator()(const ContactPairKey& key) const;
	};

	/// -------------------------------------------------------------
	///                         物理イベントディスパッチャ
	/// -------------------------------------------------------------
	class PhysicsEventDispatcher
	{
	public:
		// Contact一覧からEnter/Stay/Exitイベントを生成する。
		void Update(const std::vector<Contact>& contacts);

		// 直近Updateで生成されたイベント一覧を取得する。
		const std::vector<PhysicsEvent>& GetEvents() const { return events_; }

		// Contact履歴と生成済みイベントをクリアする。
		void Clear();

		// 物理イベント通知先を追加する。
		void AddListener(IPhysicsEventListener* listener);

		// 物理イベント通知先を削除する。
		void RemoveListener(IPhysicsEventListener* listener);

	private:
		using ContactMap = std::unordered_map<ContactPairKey, Contact, ContactPairKeyHash>;

		// Contact情報から指定種別のPhysicsEventを作る。
		PhysicsEvent BuildEvent(PhysicsEventType type, const Contact& contact) const;

		// 生成したイベントを一覧へ積み、登録済みListenerへ通知する。
		void PushEvent(const PhysicsEvent& event);

	private:
		// 前フレームで接触していたColliderペア。
		ContactMap previousContacts_{};

		// 現在フレームで接触しているColliderペア。
		ContactMap currentContacts_{};

		// 直近Updateで生成されたイベント一覧。
		std::vector<PhysicsEvent> events_{};

		// 外部通知先。所有権は持たない。
		std::vector<IPhysicsEventListener*> listeners_{};
	};

} // namespace Ken4lowEngine
