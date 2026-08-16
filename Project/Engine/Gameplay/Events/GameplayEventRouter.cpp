#include "GameplayEventRouter.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{

GameplayEventRouter* GameplayEventRouter::GetInstance()
{
	static GameplayEventRouter instance;
	return &instance;
}

GameplayEventSubscriptionHandle GameplayEventRouter::Subscribe(std::string filterTag, Callback callback, bool exactMatch)
{
	if (!callback || !GameplayTag::IsValidString(filterTag)) return {};
	GameplayEventSubscriptionHandle handle{ nextHandleValue_++ };
	if (nextHandleValue_ == 0) ++nextHandleValue_;
	subscribers_.emplace(handle.value, Subscriber{ handle, std::move(filterTag), std::move(callback), exactMatch });
	RefreshSubscriberStats();
	return handle;
}

GameplayEventSubscriptionHandle GameplayEventRouter::SubscribeAll(Callback callback)
{
	if (!callback) return {};
	GameplayEventSubscriptionHandle handle{ nextHandleValue_++ };
	if (nextHandleValue_ == 0) ++nextHandleValue_;
	subscribers_.emplace(handle.value, Subscriber{ handle, {}, std::move(callback), false });
	RefreshSubscriberStats();
	return handle;
}

bool GameplayEventRouter::Unsubscribe(GameplayEventSubscriptionHandle handle)
{
	if (!handle.IsValid()) return false;
	const bool removed = subscribers_.erase(handle.value) > 0u;
	if (removed) RefreshSubscriberStats();
	return removed;
}

uint32_t GameplayEventRouter::Publish(const GameplayEvent& event)
{
	const GameplayTag eventTag(event.eventTag);
	if (!eventTag.IsValid())
	{
		++stats_.invalidEventCount;
		return 0u;
	}

	++stats_.publishedCount;
	struct PendingDelivery
	{
		uint64_t id = 0;
		Callback callback;
	};
	std::vector<PendingDelivery> deliveries;
	deliveries.reserve(subscribers_.size());
	for (const auto& [id, subscriber] : subscribers_)
	{
		if (Matches(subscriber, eventTag) && subscriber.callback)
		{
			deliveries.push_back({ id, subscriber.callback });
		}
	}
	std::sort(deliveries.begin(), deliveries.end(),
		[](const PendingDelivery& a, const PendingDelivery& b) { return a.id < b.id; });

	uint32_t delivered = 0u;
	for (const PendingDelivery& delivery : deliveries)
	{
		// Callback自体をsnapshotしているため、Dispatch中のUnsubscribe/Clearは次回Publishからだけ反映される。
		delivery.callback(event);
		++delivered;
	}
	stats_.deliveredCount += delivered;
	return delivered;
}

void GameplayEventRouter::Clear()
{
	subscribers_.clear();
	stats_.subscriberCount = 0u;
}

bool GameplayEventRouter::Matches(const Subscriber& subscriber, const GameplayTag& eventTag) const
{
	if (subscriber.filterTag.empty()) return true;
	const GameplayTag filter(subscriber.filterTag);
	if (!filter.IsValid()) return false;
	return subscriber.exactMatch ? eventTag.MatchesExact(filter) : eventTag.Matches(filter);
}

void GameplayEventRouter::RefreshSubscriberStats()
{
	stats_.subscriberCount = static_cast<uint32_t>(subscribers_.size());
	stats_.peakSubscriberCount = (std::max)(stats_.peakSubscriberCount, stats_.subscriberCount);
}

} // namespace Ken4lowEngine
