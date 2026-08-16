#pragma once

#include "ActorHandle.h"
#include "GameplayTag.h"
#include "Vector3.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

struct GameplayEvent
{
	std::string eventTag;
	ActorHandle source{};
	ActorHandle target{};
	Vector3 worldPosition{};
	Vector3 direction{};
	float magnitude = 0.0f;
	std::string abilityName;
	std::string payloadName;
	bool hasWorldPosition = false;
	bool hasDirection = false;
};

struct GameplayEventSubscriptionHandle
{
	uint64_t value = 0;
	[[nodiscard]] bool IsValid() const { return value != 0; }
	explicit operator bool() const { return IsValid(); }
	bool operator==(const GameplayEventSubscriptionHandle&) const = default;
};

struct GameplayEventRouterStats
{
	uint64_t publishedCount = 0;
	uint64_t deliveredCount = 0;
	uint64_t invalidEventCount = 0;
	uint32_t subscriberCount = 0;
	uint32_t peakSubscriberCount = 0;
};

/// <summary>
/// Gameplay subsystem間の通知をActor生ポインタなしで配送する同期Event Router。
/// Dispatch中のSubscribe/Unsubscribeでも現在Dispatchの対象集合は変えない。
/// </summary>
class GameplayEventRouter
{
public:
	using Callback = std::function<void(const GameplayEvent&)>;

	static GameplayEventRouter* GetInstance();

	GameplayEventSubscriptionHandle Subscribe(std::string filterTag, Callback callback, bool exactMatch = false);
	GameplayEventSubscriptionHandle SubscribeAll(Callback callback);
	bool Unsubscribe(GameplayEventSubscriptionHandle handle);
	uint32_t Publish(const GameplayEvent& event);
	void Clear();

	[[nodiscard]] const GameplayEventRouterStats& GetStats() const { return stats_; }

private:
	struct Subscriber
	{
		GameplayEventSubscriptionHandle handle{};
		std::string filterTag;
		Callback callback;
		bool exactMatch = false;
	};

	GameplayEventRouter() = default;
	bool Matches(const Subscriber& subscriber, const GameplayTag& eventTag) const;
	void RefreshSubscriberStats();

	std::unordered_map<uint64_t, Subscriber> subscribers_;
	GameplayEventRouterStats stats_{};
	uint64_t nextHandleValue_ = 1;
};

} // namespace Ken4lowEngine
