#pragma once
#include "CollisionBroadPhase.h"
#include "CollisionHitResult.h"
#include "CollisionPresetLibrary.h"
#include "ObjectCollisionResponseMatrix.h"
#include "TraceResponseMatrix.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	struct Segment;
}

namespace K4E = ::Ken4lowEngine;

enum class ECollisionBroadPhaseMode : uint8_t
{
	BruteForce,
	UniformGrid,
};

struct CollisionBroadPhaseDebugComparison
{
	size_t bruteForcePairCount = 0;
	size_t uniformGridPairCount = 0;
	size_t uniformGridMissingPairCount = 0;
	size_t uniformGridDuplicatePairCount = 0;
	bool uniformGridHasMissingPairs = false;
};

struct CollisionEventPairKey
{
	uint32_t lowId = 0;
	uint32_t highId = 0;
	bool operator==(const CollisionEventPairKey& other) const { return lowId == other.lowId && highId == other.highId; }
};

struct CollisionEventPairKeyHash
{
	size_t operator()(const CollisionEventPairKey& key) const
	{
		return (static_cast<size_t>(key.lowId) << 32) ^ static_cast<size_t>(key.highId);
	}
};

struct CollisionEventContact
{
	CollisionEventPairKey key{};
	K4E::Collider* colliderA = nullptr;
	K4E::Collider* colliderB = nullptr;
	ECollisionResponse response = ECollisionResponse::Ignore;
};

/// 当たり判定管理クラス。
class CollisionManager
{
public:
	void Initialize();
	void Update();
	void Draw();
	void DrawImGui();
	void Reset();
	void CheckAllCollisions();

	void AddCollider(K4E::Collider* other);
	void RemoveCollider(K4E::Collider* other);

	/// CharacterActor系を渡した場合はActor自身ではなくCharacterColliderComponent所有Colliderを登録する。
	template<class T>
		requires requires(T* value) { value->GetCollisionPrimitive(); }
	void AddCollider(T* character)
	{
		AddCollider(character ? character->GetCollisionPrimitive() : nullptr);
	}

	/// CharacterActor系の登録解除も同じComponent所有Colliderへ統一する。
	template<class T>
		requires requires(T* value) { value->GetCollisionPrimitive(); }
	void RemoveCollider(T* character)
	{
		RemoveCollider(character ? character->GetCollisionPrimitive() : nullptr);
	}

	using CollisionFunc = std::function<bool(K4E::Collider*, K4E::Collider*)>;

	bool SegmentCast(uint32_t targetType, const K4E::Segment& seg, K4E::Collider** outHit = nullptr) const;
	bool SegmentCastHit(uint32_t targetType, const K4E::Segment& seg, CollisionHitResult& outHit) const;
	bool SegmentCastByTraceChannel(ETraceChannel traceChannel, const K4E::Segment& seg, CollisionHitResult& outHit) const;
	bool RaycastSingle(const RaycastQuery& query, RaycastHit& outHit) const;
	std::vector<RaycastHit> RaycastAll(const RaycastQuery& query) const;

	const std::vector<K4E::Collider*>& GetCollidersByType(uint32_t typeId) const
	{
		static const std::vector<K4E::Collider*> empty{};
		if (typeId >= kMaxTypes) return empty;
		return buckets_[typeId];
	}

	size_t GetColliderCount() const { return all_.size(); }
	size_t GetColliderCountByType(uint32_t typeId) const
	{
		if (typeId >= kMaxTypes) return 0;
		return buckets_[typeId].size();
	}

	ECollisionResponse GetCollisionResponse(uint32_t selfTypeId, uint32_t otherTypeId) const
	{
		return responseMatrix_.GetResponse(selfTypeId, otherTypeId);
	}

	ECollisionResponse GetCollisionResponse(EObjectChannel self, EObjectChannel other) const
	{
		return responseMatrix_.GetResponse(self, other);
	}

	const ObjectCollisionResponseMatrix& GetResponseMatrix() const { return responseMatrix_; }
	ECollisionResponse GetTraceResponse(ETraceChannel traceChannel, uint32_t objectTypeId) const
	{
		return traceResponseMatrix_.GetResponse(traceChannel, objectTypeId);
	}
	const TraceResponseMatrix& GetTraceResponseMatrix() const { return traceResponseMatrix_; }
	void SetBroadPhaseMode(ECollisionBroadPhaseMode mode) { broadPhaseMode_ = mode; }
	ECollisionBroadPhaseMode GetBroadPhaseMode() const { return broadPhaseMode_; }
	size_t CollectPotentialPairsForDebug(std::vector<CollisionPair>& outPairs) const;
	CollisionBroadPhaseDebugComparison CompareBroadPhasesForDebug() const;
	const CollisionPresetLibrary& GetCollisionPresetLibrary() const { return presetLibrary_; }

private:
	ECollisionResponse GetCollisionResponseForPair(uint32_t selfTypeId, uint32_t otherTypeId) const;
	ECollisionResponse GetCollisionResponseForCollider(K4E::Collider* self, K4E::Collider* other) const;
	ECollisionResponse ResolveCollisionResponseForPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const;
	bool ShouldSkipCollisionPair(ECollisionResponse response) const;
	bool IsCollisionIgnored(uint32_t selfTypeId, uint32_t otherTypeId) const;
	bool IsCollisionIgnored(K4E::Collider* colliderA, K4E::Collider* colliderB) const;
	bool IsColliderProcessable(K4E::Collider* collider) const;
	bool ShouldShowCollisionDebugViewer() const;
	K4E::Vector4 GetColliderDebugDrawColor(K4E::Collider* collider) const;
	bool TryGetCurrentContactResponse(K4E::Collider* collider, ECollisionResponse& outResponse) const;
	const char* GetColliderSkipReason(K4E::Collider* collider) const;
	RaycastQuery NormalizeRaycastQuery(const RaycastQuery& query) const;
	bool RaycastCollider(const RaycastQuery& query, K4E::Collider* collider, RaycastHit& outHit) const;
	bool RaycastShape(const RaycastQuery& query, K4E::Collider* collider, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastAABB(const RaycastQuery& query, const K4E::AABB& aabb, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastSphere(const RaycastQuery& query, const K4E::Sphere& sphere, float& outDistance, K4E::Vector3& outNormal) const;
	bool RaycastOBB(const RaycastQuery& query, const K4E::OBB& obb, float& outDistance, K4E::Vector3& outNormal) const;
	void RecordRaycastDebugLine(const RaycastQuery& query, const std::vector<RaycastHit>& hits) const;
	bool TestCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const;
	void UpdateContactState(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);
	CollisionEventPairKey MakeCollisionEventPairKey(K4E::Collider* colliderA, K4E::Collider* colliderB) const;
	K4E::CollisionHit BuildCollisionHit(K4E::Collider* self, K4E::Collider* other, ECollisionResponse response) const;
	void DispatchCollisionEvents();
	void DispatchActiveCollisionEvent(const CollisionEventContact& contact, bool wasTouching);
	void DispatchExitCollisionEvent(const CollisionEventContact& contact);
	void ProcessCollisionPairByResponse(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);
	std::vector<CollisionBroadPhaseTypePair> BuildLegacyBroadPhaseTypePairs() const;
	void CollectPotentialPairsWithBruteForceBroadPhase(std::vector<CollisionPair>& outPairs) const;
	void CollectPotentialPairsWithUniformGridBroadPhase(std::vector<CollisionPair>& outPairs) const;
	void ProcessBlockCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);
	void ProcessOverlapCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB);
	void CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response);
	void RegisterCollisionFuncsions();
	bool ApplyCollisionPresetToRegisteredCollider(K4E::Collider* collider, const CollisionPreset& preset);

private:
	static const uint32_t kMaxTypes = 32;
	ICollisionBroadPhase::ColliderBuckets buckets_{};
	std::vector<K4E::Collider*> all_;
	std::map<std::pair<uint32_t, uint32_t>, CollisionFunc> collisionTable_;
	ObjectCollisionResponseMatrix responseMatrix_{};
	TraceResponseMatrix traceResponseMatrix_{};
	CollisionPresetLibrary presetLibrary_{};
	BruteForceBroadPhase bruteForceBroadPhase_{};
	UniformGridBroadPhase uniformGridBroadPhase_{};
	ECollisionBroadPhaseMode broadPhaseMode_ = ECollisionBroadPhaseMode::BruteForce;
	mutable size_t lastBroadPhaseCandidatePairCount_ = 0;
	mutable CollisionBroadPhaseDebugComparison lastBroadPhaseComparison_{};
	mutable CollisionHitResult lastTraceHitResult_{};
	mutable ETraceChannel lastTraceChannel_ = ETraceChannel::Visibility;
	mutable bool lastTraceUsedTraceChannel_ = false;
	mutable bool hasLastTraceHitResult_ = false;
	uint32_t ignoredPairLoopCount_ = 0;

	using CollisionEventContactMap = std::unordered_map<CollisionEventPairKey, CollisionEventContact, CollisionEventPairKeyHash>;
	CollisionEventContactMap previousContacts_{};
	CollisionEventContactMap currentContacts_{};
	uint32_t lastCollisionEnterEventCount_ = 0;
	uint32_t lastCollisionStayEventCount_ = 0;
	uint32_t lastCollisionExitEventCount_ = 0;
	uint32_t lastOverlapBeginEventCount_ = 0;
	uint32_t lastOverlapStayEventCount_ = 0;
	uint32_t lastOverlapEndEventCount_ = 0;

	struct RaycastDebugLine
	{
		K4E::Vector3 start{};
		K4E::Vector3 end{};
		K4E::Vector4 color{};
		bool hit = false;
	};

	mutable std::vector<RaycastDebugLine> raycastDebugLines_{};
	mutable RaycastQuery lastRaycastQuery_{};
	mutable std::vector<RaycastHit> lastRaycastHits_{};
	mutable bool hasLastRaycastQuery_ = false;
	bool isCollider_ = true;
};
