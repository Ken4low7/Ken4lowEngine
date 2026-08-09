#pragma once
#include <cstdint>

#include "CollisionTypes.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Collider;
}

namespace K4E = ::Ken4lowEngine;

/// Raycast / SegmentCast / Sweepなどの単発問い合わせ結果を保持する。
struct CollisionHitResult
{
	bool hit = false;
	K4E::Vector3 point{};  // 現段階で正確な交点が取れない問い合わせは近似点を入れる。
	K4E::Vector3 normal{}; // 法線はPrimitive判定が返せる段階で埋める。
	float distance = 0.0f;
	K4E::Collider* collider = nullptr;
	uint32_t typeId = 0u;
	EObjectChannel objectChannel = EObjectChannel::Default;
	ECollisionResponse response = ECollisionResponse::Ignore;
};

/// RaycastQuery はRay/Trace問い合わせの入力条件をまとめる。
/// Collision Eventとは独立した読み取り専用問い合わせとして扱う。
struct RaycastQuery
{
	K4E::Vector3 origin{};
	K4E::Vector3 direction{ 0.0f, 0.0f, 1.0f };
	float maxDistance = 1000.0f;
	ETraceChannel traceChannel = ETraceChannel::Visibility;
};

/// RaycastHit はRaycastで得た命中情報を距離順に扱うための結果データ。
struct RaycastHit
{
	bool hit = false;
	K4E::Vector3 point{};
	K4E::Vector3 normal{};
	float distance = 0.0f;
	K4E::Collider* collider = nullptr;
	uint32_t typeId = 0u;
	EObjectChannel objectChannel = EObjectChannel::Default;
	ECollisionResponse response = ECollisionResponse::Ignore;
};
