#include "CollisionManager.h"
#include "ParameterManager.h" // Collision判定ではなく、Colliderワイヤー表示フラグの外側管理にだけ使う。
#include "Collider.h"
#include <Editor/EditorModeController.h>
#include <CollisionUtility.h>
#include <CollisionTypeIdDef.h>
#include <Wireframe.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace K4E = ::Ken4lowEngine;

namespace
{
	const char* ToString(ECollisionResponse response)
	{
		switch (response)
		{
		case ECollisionResponse::Ignore: return "Ignore";
		case ECollisionResponse::Overlap: return "Overlap";
		case ECollisionResponse::Block: return "Block";
		default: return "Unknown";
		}
	}

	const char* ToString(ETraceChannel channel)
	{
		switch (channel)
		{
		case ETraceChannel::Visibility: return "Visibility";
		case ETraceChannel::Camera: return "Camera";
		case ETraceChannel::Weapon: return "Weapon";
		case ETraceChannel::AI: return "AI";
		case ETraceChannel::Interaction: return "Interaction";
		default: return "Unknown";
		}
	}

	const char* ToString(EObjectChannel channel)
	{
		switch (channel)
		{
		case EObjectChannel::Default: return "Default";
		case EObjectChannel::Player: return "Player";
		case EObjectChannel::Weapon: return "Weapon";
		case EObjectChannel::Enemy: return "Enemy";
		case EObjectChannel::PlayerProjectile: return "PlayerProjectile";
		case EObjectChannel::EnemyProjectile: return "EnemyProjectile";
		case EObjectChannel::Item: return "Item";
		case EObjectChannel::Dummy: return "Dummy";
		case EObjectChannel::Boss: return "Boss";
		case EObjectChannel::BossProjectile: return "BossProjectile";
		case EObjectChannel::WorldStatic: return "WorldStatic";
		case EObjectChannel::TargetLock: return "TargetLock";
		case EObjectChannel::Crystal: return "Crystal";
		default: return "Unknown";
		}
	}

	const char* ToString(K4E::ECollisionShapeType shapeType)
	{
		switch (shapeType)
		{
		case K4E::ECollisionShapeType::None: return "None";
		case K4E::ECollisionShapeType::Sphere: return "Sphere";
		case K4E::ECollisionShapeType::AABB: return "AABB";
		case K4E::ECollisionShapeType::OBB: return "OBB";
		case K4E::ECollisionShapeType::Capsule: return "Capsule";
		case K4E::ECollisionShapeType::Segment: return "Segment";
		default: return "Unknown";
		}
	}

	const std::array<EObjectChannel, 13>& DebugObjectChannels()
	{
		static const std::array<EObjectChannel, 13> channels = {
			EObjectChannel::Default,
			EObjectChannel::Player,
			EObjectChannel::Weapon,
			EObjectChannel::Enemy,
			EObjectChannel::PlayerProjectile,
			EObjectChannel::EnemyProjectile,
			EObjectChannel::Item,
			EObjectChannel::Dummy,
			EObjectChannel::Boss,
			EObjectChannel::BossProjectile,
			EObjectChannel::WorldStatic,
			EObjectChannel::TargetLock,
			EObjectChannel::Crystal,
		};
		return channels;
	}

	uint64_t MakeDebugCollisionPairKey(const CollisionPair& pair)
	{
		const uint32_t idA = pair.a ? pair.a->GetUniqueID() : 0;
		const uint32_t idB = pair.b ? pair.b->GetUniqueID() : 0;
		const uint32_t lo = idA < idB ? idA : idB;
		const uint32_t hi = idA < idB ? idB : idA;
		return (static_cast<uint64_t>(lo) << 32) | hi;
	}

	size_t CountDuplicatePairsForDebug(const std::vector<CollisionPair>& pairs, std::unordered_set<uint64_t>& outUniqueKeys)
	{
		outUniqueKeys.clear();
		size_t duplicateCount = 0;
		for (const CollisionPair& pair : pairs)
		{
			// Debug比較ではUniqueID順のキーに正規化し、BroadPhase実装ごとのペア順差を無視する。
			const uint64_t key = MakeDebugCollisionPairKey(pair);
			if (!outUniqueKeys.insert(key).second)
			{
				++duplicateCount;
			}
		}
		return duplicateCount;
	}
}

/// -------------------------------------------------------------
///                         初期化処理
/// -------------------------------------------------------------
void CollisionManager::Initialize()
{
#ifdef _DEBUG
	isCollider_ = true;
#else
	isCollider_ = false;
#endif
	K4E::ParameterManager::GetInstance()->CreateGroup("K4E::Collider");
	K4E::ParameterManager::GetInstance()->AddItem("K4E::Collider", "isCollider", isCollider_);
	K4E::ParameterManager::GetInstance()->SetDisplayName("K4E::Collider", "isCollider", "コライダー表示");
	K4E::ParameterManager::GetInstance()->RegisterParameterApplier("K4E::Collider", this, [this]() {
#ifdef _DEBUG
		isCollider_ = K4E::ParameterManager::GetInstance()->GetValue<bool>("K4E::Collider", "isCollider");
#else
		isCollider_ = false;
#endif
	}); // ParameterManager依存はDebug可視化フラグに限定し、Preset/判定処理には接続しない。

	// 既存ペア表と同じ関係をResponseMatrixへ写し、Ignore/Block/Overlapの入口に使う。
	responseMatrix_.InitializeLegacyDefaults();

	// TraceChannelごとの問い合わせ対象を初期化し、既存SegmentCastとは別入口で使う。
	traceResponseMatrix_.InitializeLegacyDefaults();

	// CollisionPreset Jsonは読み込みに失敗してもコード既定値へフォールバックし、既存Collider設定は自動変更しない。
	presetLibrary_.LoadFromJsonFile();

	// 衝突判定関数の登録
	RegisterCollisionFuncsions();
}

/// -------------------------------------------------------------
///                         更新処理
/// -------------------------------------------------------------
void CollisionManager::Update()
{
#ifdef _DEBUG
	isCollider_ = K4E::ParameterManager::GetInstance()->GetValue<bool>("K4E::Collider", "isCollider");
#else
	// Releaseビルドでは保存済みパラメータがtrueでもColliderワイヤーを復活させない。
	isCollider_ = false;
#endif

	// Collider 本体の Update はデバッグ用（Wireframeなど）
	for (K4E::Collider* collider : all_)
	{
		if (IsColliderProcessable(collider)) collider->Update();
	}
}

/// -------------------------------------------------------------
///                         描画処理
/// -------------------------------------------------------------
void CollisionManager::Draw()
{
#ifndef _DEBUG
	return;
#endif
	if (!ShouldShowCollisionDebugViewer()) return;
	if (!isCollider_) return;

	for (K4E::Collider* collider : all_)
	{
		if (!collider) continue;
		collider->DrawDebug(GetColliderDebugDrawColor(collider), true);
	}

	for (const RaycastDebugLine& line : raycastDebugLines_)
	{
		K4E::Segment segment{};
		segment.origin = line.start;
		segment.diff = line.end - line.start;
		K4E::Wireframe::GetInstance()->DrawSegment(segment, line.color);
	}
}


void CollisionManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ShouldShowCollisionDebugViewer()) return;

	// Collision DebugではCollider表示フラグと登録状況を通常Dockウィンドウ内に表示する。
	bool showCollider = isCollider_;
	if (ImGui::Checkbox("Show Collider", &showCollider))
	{
		isCollider_ = showCollider;
		K4E::ParameterManager::GetInstance()->SetValue("K4E::Collider", "isCollider", isCollider_);
	}
	ImGui::Text("All Colliders: %d", static_cast<int>(all_.size()));
	ImGui::Text("Ignored Pair Loops: %u", ignoredPairLoopCount_);
	if (ImGui::TreeNode("Raycast / Trace Debug"))
	{
		ImGui::Text("Last Query: %s", hasLastRaycastQuery_ ? "Raycast" : "None");
		if (hasLastRaycastQuery_)
		{
			ImGui::Text("Origin: %.2f, %.2f, %.2f", lastRaycastQuery_.origin.x, lastRaycastQuery_.origin.y, lastRaycastQuery_.origin.z);
			ImGui::Text("Direction: %.2f, %.2f, %.2f", lastRaycastQuery_.direction.x, lastRaycastQuery_.direction.y, lastRaycastQuery_.direction.z);
			ImGui::Text("Max Distance: %.2f", lastRaycastQuery_.maxDistance);
			ImGui::Text("Trace Channel: %s", ToString(lastRaycastQuery_.traceChannel));
			ImGui::Text("Hit Count: %d", static_cast<int>(lastRaycastHits_.size()));
			for (const RaycastHit& hit : lastRaycastHits_)
			{
				ImGui::Text("#%u %s %s dist %.2f point %.2f, %.2f, %.2f",
					hit.collider ? hit.collider->GetUniqueID() : 0,
					ToString(hit.objectChannel),
					ToString(hit.response),
					hit.distance,
					hit.point.x, hit.point.y, hit.point.z);
			}
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Debug Draw Legend"))
	{
		ImGui::TextColored(ImVec4(0.20f, 0.95f, 0.35f, 1.0f), "Enabled Collider");
		ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "Disabled / not processable");
		ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.10f, 1.0f), "Trigger / Overlap");
		ImGui::TextColored(ImVec4(1.0f, 0.18f, 0.12f, 1.0f), "Block contact");
		ImGui::TextColored(ImVec4(0.18f, 0.18f, 0.18f, 1.0f), "Ignore / query off / missing dynamic owner");
		ImGui::Text("Center marker and derived AABB are drawn as helper guides.");
		ImGui::TreePop();
	}

#ifdef USE_IMGUI
	if (K4E::EditorModeController::GetInstance()->ShouldDrawEditorUi() && ImGui::TreeNode("Collision Event Debug"))
	{
		// Game Preview Modeでは表示せず、Editor Mode中だけイベント配送の概況を確認する。
		ImGui::Text("Current/Previous Contact Pairs: %d / %d",
			static_cast<int>(currentContacts_.size()),
			static_cast<int>(previousContacts_.size()));
		ImGui::Text("Collision Enter/Stay/Exit: %u / %u / %u",
			lastCollisionEnterEventCount_,
			lastCollisionStayEventCount_,
			lastCollisionExitEventCount_);
		ImGui::Text("Overlap Begin/Stay/End: %u / %u / %u",
			lastOverlapBeginEventCount_,
			lastOverlapStayEventCount_,
			lastOverlapEndEventCount_);

		static int eventDebugDisplayLimit = 32;
		ImGui::DragInt("Pair Display Limit", &eventDebugDisplayLimit, 1.0f, 1, 256);
		int displayedPairCount = 0;
		if (ImGui::TreeNode("Current Contact Pairs"))
		{
			for (const auto& [key, contact] : currentContacts_)
			{
				if (displayedPairCount >= eventDebugDisplayLimit)
				{
					ImGui::TextDisabled("Hidden Pairs: %d", static_cast<int>(currentContacts_.size()) - displayedPairCount);
					break;
				}
				ImGui::Text("#%u <-> #%u  %s  A:%u B:%u",
					key.lowId,
					key.highId,
					ToString(contact.response),
					contact.colliderA ? contact.colliderA->GetTypeID() : 0,
					contact.colliderB ? contact.colliderB->GetTypeID() : 0);
				++displayedPairCount;
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("BroadPhase Debug"))
	{
		// BroadPhase切り替えはDebug候補収集にだけ使い、本番CheckAllCollisionsにはまだ反映しない。
		int mode = static_cast<int>(broadPhaseMode_);
		if (ImGui::RadioButton("BruteForce", mode == static_cast<int>(ECollisionBroadPhaseMode::BruteForce)))
		{
			SetBroadPhaseMode(ECollisionBroadPhaseMode::BruteForce);
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("UniformGrid(test)", mode == static_cast<int>(ECollisionBroadPhaseMode::UniformGrid)))
		{
			SetBroadPhaseMode(ECollisionBroadPhaseMode::UniformGrid);
		}

		ImGui::Text("Candidate Pairs: %d", static_cast<int>(lastBroadPhaseCandidatePairCount_));
		ImGui::Text("BF/UG Pairs: %d / %d",
			static_cast<int>(lastBroadPhaseComparison_.bruteForcePairCount),
			static_cast<int>(lastBroadPhaseComparison_.uniformGridPairCount));
		ImGui::Text("UG Missing/Duplicate: %d / %d",
			static_cast<int>(lastBroadPhaseComparison_.uniformGridMissingPairCount),
			static_cast<int>(lastBroadPhaseComparison_.uniformGridDuplicatePairCount));
		ImGui::Text("UG Cell Size: %.2f", uniformGridBroadPhase_.GetCellSize());
		ImGui::Text("UG Registered/Cells: %d / %d",
			static_cast<int>(uniformGridBroadPhase_.GetLastRegisteredColliderCount()),
			static_cast<int>(uniformGridBroadPhase_.GetLastUsedCellCount()));
		ImGui::Text("UG Last Candidates: %d", static_cast<int>(uniformGridBroadPhase_.GetLastCandidatePairCount()));
		if (lastBroadPhaseComparison_.uniformGridHasMissingPairs)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "UniformGrid has missing pairs. Keep BruteForce for runtime.");
		}
		if (ImGui::Button("Refresh BroadPhase Candidates"))
		{
			std::vector<CollisionPair> debugPairs;
			CollectPotentialPairsForDebug(debugPairs);
		}
		if (ImGui::Button("Compare BroadPhase Candidates"))
		{
			CompareBroadPhasesForDebug();
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Trace / HitResult Debug"))
	{
		// 最後に実行されたTrace系問い合わせの結果だけを表示し、ログ量を増やさない。
		ImGui::Text("Last Query: %s", hasLastTraceHitResult_ ? (lastTraceUsedTraceChannel_ ? "TraceChannel SegmentCast" : "TypeID SegmentCast") : "None");
		ImGui::Text("TraceChannel: %s", lastTraceUsedTraceChannel_ ? ToString(lastTraceChannel_) : "N/A");
		ImGui::Text("QueryParams: not implemented yet");
		ImGui::Text("Hit: %s", lastTraceHitResult_.hit ? "true" : "false");
		ImGui::Text("Hit Collider: %p", static_cast<void*>(lastTraceHitResult_.collider));
		ImGui::Text("Hit Type/Object: %u / %s", lastTraceHitResult_.typeId, ToString(lastTraceHitResult_.objectChannel));
		ImGui::Text("Hit Response: %s", ToString(lastTraceHitResult_.response));
		ImGui::Text("Hit Point: %.2f, %.2f, %.2f", lastTraceHitResult_.point.x, lastTraceHitResult_.point.y, lastTraceHitResult_.point.z);
		ImGui::Text("Hit Distance: %.3f", lastTraceHitResult_.distance);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("CollisionPreset Library"))
	{
		// Preset一覧は確認と明示保存/再読み込みだけに留め、既存Colliderへ自動反映しない。
		const auto& presets = presetLibrary_.GetPresets();
		ImGui::Text("Json Path: %s", CollisionPresetLibrary::kDefaultJsonPath);
		ImGui::Text("Source: %s", presetLibrary_.WasLoadedFromJson() ? "Json" : "Built-in defaults");
		ImGui::TextWrapped("Status: %s", presetLibrary_.GetLastStatus().c_str());
		ImGui::Text("Preset Count: %d", static_cast<int>(presets.size()));
		if (ImGui::Button("Reload CollisionPreset Json"))
		{
			presetLibrary_.LoadFromJsonFile();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save CollisionPreset Json"))
		{
			presetLibrary_.SaveToJsonFile();
		}

		for (const CollisionPreset& preset : presets)
		{
			if (ImGui::TreeNode(preset.name.c_str()))
			{
				ImGui::Text("ObjectChannel: %s", ToString(preset.objectChannel));
				ImGui::Text("Query/Physics: %s / %s", preset.queryEnabled ? "true" : "false", preset.physicsEnabled ? "true" : "false");
				if (ImGui::TreeNode("Responses"))
				{
					for (EObjectChannel otherChannel : DebugObjectChannels())
					{
						ImGui::Text("%s: %s", ToString(otherChannel), ToString(preset.GetResponse(otherChannel)));
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif // USE_IMGUI

	if (ImGui::TreeNode("Collider Buckets"))
	{
		for (uint32_t i = 0; i < kMaxTypes; ++i)
		{
			if (!buckets_[i].empty())
			{
				ImGui::Text("Type %u: %d", i, static_cast<int>(buckets_[i].size()));
			}
		}
		ImGui::TreePop();
	}

#ifdef USE_IMGUI
	if (ImGui::TreeNode("Collider Details"))
	{
		// Collider詳細は数が増えると重くなるため、Debug表示件数を明示的に制限する。
		static int colliderDebugDisplayLimit = 64;
		ImGui::DragInt("Display Limit", &colliderDebugDisplayLimit, 1.0f, 1, 512);
		int displayedColliderCount = 0;

		for (K4E::Collider* collider : all_)
		{
			if (!collider) continue;
			if (displayedColliderCount >= colliderDebugDisplayLimit)
			{
				ImGui::TextDisabled("Hidden Colliders: %d", static_cast<int>(all_.size()) - displayedColliderCount);
				break;
			}

			const uint32_t typeId = collider->GetTypeID();
			const EObjectChannel objectChannel = ToObjectChannel(typeId);
			char label[64]{};
			snprintf(label, sizeof(label), "Collider #%u (%s)", collider->GetUniqueID(), ToString(objectChannel));
			if (ImGui::TreeNode(label))
			{
				// Colliderの基本状態を読み取り専用で表示し、判定設定の調査に使う。
				const std::string_view presetName = collider->GetCollisionPresetName();
				const uint32_t objectChannelId = collider->GetObjectChannelId();
				const EObjectChannel colliderObjectChannel = ToObjectChannel(objectChannelId);
				const K4E::Vector3 center = collider->GetCenterPosition();
				const K4E::Vector3 halfSize = collider->GetOBBHalfSize();
				const std::string_view ownerName = collider->GetOwnerDebugName();
				ImGui::Text("Owner: %s (%p)",
					ownerName.empty() ? "(none/world/static)" : ownerName.data(),
					collider->GetOwner<void>());
				ImGui::Text("Registered: true");
				ImGui::Text("Enabled/Processable: %s / %s",
					collider->IsEnabled() ? "true" : "false",
					IsColliderProcessable(collider) ? "true" : "false");
				ImGui::Text("Skip Reason: %s", GetColliderSkipReason(collider));
				ImGui::Text("ShapeType: %s", ToString(collider->GetShapeType()));
				ImGui::Text("TypeID/ObjectChannel: %u / %s(%u)", typeId, ToString(colliderObjectChannel), objectChannelId);
				if (presetName.empty())
				{
					ImGui::Text("CollisionPreset: (manual/unset)");
				}
				else
				{
					ImGui::Text("CollisionPreset: %.*s", static_cast<int>(presetName.size()), presetName.data());
				}
				ImGui::Text("Query/Physics: %s / %s", collider->IsQueryEnabled() ? "true" : "false", collider->IsPhysicsEnabled() ? "true" : "false");
				ImGui::Text("Response Source: %s", collider->HasCollisionResponseOverrides() ? "Collider Preset" : "Legacy Matrix fallback");
				ImGui::Text("Trigger: %s", collider->IsTrigger() ? "true" : "false");
				ImGui::Text("Owner Required/Active/Alive/Visible: %s / %s / %s / %s",
					collider->RequiresOwner() ? "true" : "false",
					collider->IsOwnerActive() ? "true" : "false",
					collider->IsOwnerAlive() ? "true" : "false",
					collider->IsOwnerVisible() ? "true" : "false");
				ImGui::Text("DebugDraw: %s", isCollider_ ? "true" : "false");
				ImGui::Text("Center: %.2f, %.2f, %.2f", center.x, center.y, center.z);
				ImGui::Text("HalfSize: %.2f, %.2f, %.2f", halfSize.x, halfSize.y, halfSize.z);
				ImGui::Text("AABB Min: %.2f, %.2f, %.2f", collider->GetAABB().min.x, collider->GetAABB().min.y, collider->GetAABB().min.z);
				ImGui::Text("AABB Max: %.2f, %.2f, %.2f", collider->GetAABB().max.x, collider->GetAABB().max.y, collider->GetAABB().max.z);
				if (collider->GetShapeType() == K4E::ECollisionShapeType::Sphere)
				{
					const K4E::Sphere sphere = collider->GetSphere();
					ImGui::Text("Sphere: center %.2f, %.2f, %.2f / r %.2f", sphere.center.x, sphere.center.y, sphere.center.z, sphere.radius);
				}
				if (collider->GetShapeType() == K4E::ECollisionShapeType::Segment)
				{
					const K4E::Segment segment = collider->GetSegment();
					ImGui::Text("Segment Origin: %.2f, %.2f, %.2f", segment.origin.x, segment.origin.y, segment.origin.z);
					ImGui::Text("Segment Diff: %.2f, %.2f, %.2f", segment.diff.x, segment.diff.y, segment.diff.z);
				}
				if (collider->HasCapsule())
				{
					const K4E::Capsule capsule = collider->GetCapsule();
					ImGui::Text("Capsule: center %.2f, %.2f, %.2f / r %.2f / h %.2f",
						capsule.GetCenter().x, capsule.GetCenter().y, capsule.GetCenter().z,
						capsule.radius, capsule.GetHeight());
				}
				ImGui::Text("Current/Prev Contacts: %d / %d",
					static_cast<int>(collider->GetCurrentCollisions().size()),
					static_cast<int>(collider->GetPrevCollisions().size()));

				if (ImGui::TreeNode("Current Contact Partners"))
				{
					int contactCount = 0;
					for (const auto& [_, contact] : currentContacts_)
					{
						K4E::Collider* other = nullptr;
						if (contact.colliderA == collider) other = contact.colliderB;
						if (contact.colliderB == collider) other = contact.colliderA;
						if (!other) continue;

						ImGui::Text("Other #%u  Type:%u  Channel:%s  Response:%s",
							other->GetUniqueID(),
							other->GetTypeID(),
							ToString(ToObjectChannel(other->GetObjectChannelId())),
							ToString(contact.response));
						++contactCount;
					}
					if (contactCount == 0)
					{
						ImGui::TextDisabled("No current contacts.");
					}
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Response To ObjectChannels"))
				{
					for (EObjectChannel otherChannel : DebugObjectChannels())
					{
						const ECollisionResponse response = collider->HasCollisionResponseOverrides()
							? static_cast<ECollisionResponse>(collider->GetCollisionResponseId(ToCollisionTypeId(otherChannel)))
							: responseMatrix_.GetResponse(colliderObjectChannel, otherChannel);
						if (response == ECollisionResponse::Ignore)
						{
							ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "%s: %s (will not create collision events)", ToString(otherChannel), ToString(response));
						}
						else
						{
							ImGui::Text("%s: %s", ToString(otherChannel), ToString(response));
						}
					}
					ImGui::TreePop();
				}

				const auto& presets = presetLibrary_.GetPresets();
				if (!presets.empty() && ImGui::TreeNode("Apply CollisionPreset"))
				{
					// Preset適用は選択Colliderへの明示操作だけに限定し、全Colliderへ自動反映しない。
					static int selectedPresetIndex = 0;
					if (selectedPresetIndex < 0 || selectedPresetIndex >= static_cast<int>(presets.size()))
					{
						selectedPresetIndex = 0;
					}

					const CollisionPreset& selectedPreset = presets[static_cast<size_t>(selectedPresetIndex)];
					if (ImGui::BeginCombo("Preset", selectedPreset.name.c_str()))
					{
						for (int i = 0; i < static_cast<int>(presets.size()); ++i)
						{
							const bool selected = (selectedPresetIndex == i);
							if (ImGui::Selectable(presets[static_cast<size_t>(i)].name.c_str(), selected))
							{
								selectedPresetIndex = i;
							}
							if (selected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}

					ImGui::Text("Apply Scope: this Collider only");
					if (ImGui::Button("Apply Selected Preset"))
					{
						ApplyCollisionPresetToRegisteredCollider(collider, selectedPreset);
					}
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}

			++displayedColliderCount;
		}
		ImGui::TreePop();
	}
#endif // USE_IMGUI
#endif
}

/// -------------------------------------------------------------
///                         リセット処理
/// -------------------------------------------------------------
void CollisionManager::Reset()
{
	all_.clear();
	for (auto& v : buckets_) v.clear();
	previousContacts_.clear();
	currentContacts_.clear();
}

/// -------------------------------------------------------------
///                 すべての当たり判定を確認する処理
/// -------------------------------------------------------------
void CollisionManager::CheckAllCollisions()
{
	using CId = uint32_t;
	const CId kPlayer = static_cast<CId>(CollisionTypeIdDef::kPlayer);
	const CId kEnemy = static_cast<CId>(CollisionTypeIdDef::kEnemy);
	const CId kBoss = static_cast<CId>(CollisionTypeIdDef::kBoss);
	const CId kBullet = static_cast<CId>(CollisionTypeIdDef::kBullet);
	const CId kEnemyBullet = static_cast<CId>(CollisionTypeIdDef::kEnemyBullet);
	const CId kBossBullet = static_cast<CId>(CollisionTypeIdDef::kBossBullet);
	const CId kItem = static_cast<CId>(CollisionTypeIdDef::kItem);
	const CId kWorld = static_cast<CId>(CollisionTypeIdDef::kWorld);
	const CId kCrystal = static_cast<CId>(CollisionTypeIdDef::kCrystal);
	ignoredPairLoopCount_ = 0;

	// --- スナップショット（イベント中の追加/削除に備える） ---
	std::vector<K4E::Collider*> snapshot = all_;

	// --- 1) フレーム開始：接触ペア履歴とCollider別Debug履歴をローテーション ---
	previousContacts_ = std::move(currentContacts_);
	currentContacts_.clear();
	lastCollisionEnterEventCount_ = 0;
	lastCollisionStayEventCount_ = 0;
	lastCollisionExitEventCount_ = 0;
	lastOverlapBeginEventCount_ = 0;
	lastOverlapStayEventCount_ = 0;
	lastOverlapEndEventCount_ = 0;

	for (K4E::Collider* c : snapshot)
	{
		if (c) c->BeginCollisionFrame();
	}

	// --- 2) 判定：当たったペアはResponse付きで現在フレームの接触として登録 ---
	// Phase 13設計メモ: 将来のBroad Phaseでは、この固定TypeIDペア列挙とbucket二重ループを候補ペア収集へ置き換える。
	auto pairLoop = [&](CId aId, CId bId)
		{
			const ECollisionResponse response = GetCollisionResponseForPair(aId, bId);

			// ResponseMatrixでIgnoreのペアだけを、既存と同じ「判定しない」扱いで早期終了する。
			if (ShouldSkipCollisionPair(response))
			{
				++ignoredPairLoopCount_;
				return;
			}

			auto& A = buckets_[aId];
			auto& B = buckets_[bId];
			if (A.empty() || B.empty()) return;

			// 現在はTypeIDバケット内の総当たりで候補ペアを作るため、Collider数増加時はここがO(n*m)の主な負荷になる。
			for (K4E::Collider* a : A)
			{
				if (!IsColliderProcessable(a)) continue;
				for (K4E::Collider* b : B)
				{
					if (!IsColliderProcessable(b)) continue;
					const ECollisionResponse pairResponse = ResolveCollisionResponseForPair(a, b);
					if (ShouldSkipCollisionPair(pairResponse))
					{
						++ignoredPairLoopCount_;
						continue;
					}
					ProcessCollisionPairByResponse(a, b, pairResponse);
				}
			}
		};

	// ここは片方向だけ回す（CheckCollisionPair 内で両者に登録するため）
	pairLoop(kBoss, kPlayer);
	pairLoop(kEnemy, kPlayer);
	pairLoop(kBullet, kEnemy);
	pairLoop(kBullet, kCrystal);
	pairLoop(kBoss, kBullet);
	pairLoop(kEnemyBullet, kPlayer);
	pairLoop(kPlayer, kBossBullet);
	pairLoop(kPlayer, kItem);
	pairLoop(kPlayer, kWorld);
	pairLoop(kEnemy, kWorld);
	pairLoop(kBoss, kWorld);
	// Bullet vs World（壁に当てて消す）
	pairLoop(kBullet, kWorld);
	pairLoop(kEnemyBullet, kWorld);
	pairLoop(kBossBullet, kWorld);

	// --- 3) Enter/Stay/Exit を解決して通知 ---
	DispatchCollisionEvents();
}

/// -------------------------------------------------------------
///                     コライダーを追加
/// -------------------------------------------------------------
void CollisionManager::AddCollider(K4E::Collider* other)
{
	if (!other) return;
	if (std::find(all_.begin(), all_.end(), other) != all_.end()) return;

	// 同一Colliderの二重登録を防ぎ、弾発射後にCollision負荷が増え続けないようにする。
	all_.push_back(other);
	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes) buckets_[id].push_back(other);
}

/// -------------------------------------------------------------
///                     コライダーを削除
/// -------------------------------------------------------------
void CollisionManager::RemoveCollider(K4E::Collider* other)
{
	if (!other) return;
	all_.erase(std::remove(all_.begin(), all_.end(), other), all_.end());

	const uint32_t id = other->GetTypeID();
	if (id < kMaxTypes)
	{
		auto& v = buckets_[id];
		v.erase(std::remove(v.begin(), v.end(), other), v.end());
	}

	const uint32_t uniqueId = other->GetUniqueID();
	auto removeContact = [uniqueId](CollisionEventContactMap& contacts)
		{
			for (auto it = contacts.begin(); it != contacts.end(); )
			{
				if (it->first.lowId == uniqueId || it->first.highId == uniqueId)
				{
					it = contacts.erase(it);
				}
				else
				{
					++it;
				}
			}
		};

	removeContact(previousContacts_);
	removeContact(currentContacts_);
}

bool CollisionManager::ApplyCollisionPresetToRegisteredCollider(K4E::Collider* collider, const CollisionPreset& preset)
{
	if (!collider) return false;

	const bool isRegistered = std::find(all_.begin(), all_.end(), collider) != all_.end();
	const uint32_t oldTypeId = collider->GetTypeID();
	if (isRegistered && oldTypeId < kMaxTypes)
	{
		auto& oldBucket = buckets_[oldTypeId];
		oldBucket.erase(std::remove(oldBucket.begin(), oldBucket.end(), collider), oldBucket.end());
	}

	ApplyCollisionPreset(*collider, preset);

	const uint32_t newTypeId = collider->GetTypeID();
	if (isRegistered && newTypeId < kMaxTypes)
	{
		auto& newBucket = buckets_[newTypeId];
		if (std::find(newBucket.begin(), newBucket.end(), collider) == newBucket.end())
		{
			newBucket.push_back(collider);
		}
	}

	return true;
}

bool CollisionManager::SegmentCast(uint32_t targetType, const K4E::Segment& seg, K4E::Collider** outHit) const
{
	CollisionHitResult hitResult{};
	const bool hit = SegmentCastHit(targetType, seg, hitResult);
	if (outHit) *outHit = hitResult.collider;
	return hit;
}

bool CollisionManager::SegmentCastHit(uint32_t targetType, const K4E::Segment& seg, CollisionHitResult& outHit) const
{
	// HitResult版でも既存SegmentCastと同じclosest判定基準を使い、互換挙動を維持する。
	outHit = CollisionHitResult{};
#ifdef _DEBUG
	// 最終Trace結果の保存はDebug表示専用に限定し、Releaseの問い合わせ処理へ余計な状態更新を載せない。
	lastTraceUsedTraceChannel_ = false;
	hasLastTraceHitResult_ = true;
#endif
	if (targetType >= kMaxTypes)
	{
#ifdef _DEBUG
		lastTraceHitResult_ = outHit;
#endif
		return false;
	}

	float best = (std::numeric_limits<float>::max)();
	K4E::Collider* bestCol = nullptr;
	K4E::Vector3 bestCenter{};

	for (K4E::Collider* c : buckets_[targetType])
	{
		if (!IsColliderProcessable(c)) continue;

		// Segment vs OBB 判定（既存式をPrimitive名付き入口から利用）
		if (!K4E::CollisionPrimitiveTests::TestOBBSegment(c->GetOBB(), seg)) continue;

		// “近いものを優先” の簡易（最短でそれっぽく）
		const K4E::Vector3 d = c->GetCenterPosition() - seg.origin;
		const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
		if (distSq < best)
		{
			best = distSq;
			bestCol = c;
			bestCenter = c->GetCenterPosition();
		}
	}

	if (!bestCol)
	{
#ifdef _DEBUG
		lastTraceHitResult_ = outHit;
#endif
		return false;
	}

	outHit.hit = true;
	outHit.collider = bestCol;
	outHit.typeId = bestCol->GetTypeID();
	outHit.objectChannel = ToObjectChannel(outHit.typeId);
	outHit.response = ECollisionResponse::Block; // TraceChannel未導入のため、既存SegmentCast命中は遮蔽/命中扱いに寄せる。
	outHit.distance = std::sqrt(best);
	outHit.point = bestCenter; // TODO: OBB vs Segmentが交点を返せるようになったら正確な衝突点へ置き換える。
	outHit.normal = {}; // TODO: 形状問い合わせが法線を返せる段階で設定する。
#ifdef _DEBUG
	lastTraceHitResult_ = outHit;
#endif
	return true;
}

bool CollisionManager::SegmentCastByTraceChannel(ETraceChannel traceChannel, const K4E::Segment& seg, CollisionHitResult& outHit) const
{
	// TraceChannel版は用途別Matrixで対象ObjectChannelを選び、closest hit 1件だけを返す。
	outHit = CollisionHitResult{};
#ifdef _DEBUG
	// TraceChannelのDebug表示に必要な最終問い合わせ情報だけをDebugビルドで保持する。
	lastTraceUsedTraceChannel_ = true;
	lastTraceChannel_ = traceChannel;
	hasLastTraceHitResult_ = true;
#endif

	float best = (std::numeric_limits<float>::max)();
	K4E::Collider* bestCol = nullptr;
	K4E::Vector3 bestCenter{};
	ECollisionResponse bestResponse = ECollisionResponse::Ignore;

	for (uint32_t typeId = 0; typeId < kMaxTypes; ++typeId)
	{
		const ECollisionResponse response = traceResponseMatrix_.GetResponse(traceChannel, typeId);
		if (response == ECollisionResponse::Ignore) continue;

		for (K4E::Collider* c : buckets_[typeId])
		{
			if (!IsColliderProcessable(c)) continue;

			// Segment vs OBB 判定のみを使い、既存SegmentCastと同じ形状問い合わせに揃える。
			if (!K4E::CollisionPrimitiveTests::TestOBBSegment(c->GetOBB(), seg)) continue;

			const K4E::Vector3 d = c->GetCenterPosition() - seg.origin;
			const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
			if (distSq < best)
			{
				best = distSq;
				bestCol = c;
				bestCenter = c->GetCenterPosition();
				bestResponse = response;
			}
		}
	}

	if (!bestCol)
	{
#ifdef _DEBUG
		lastTraceHitResult_ = outHit;
#endif
		return false;
	}

	outHit.hit = true;
	outHit.collider = bestCol;
	outHit.typeId = bestCol->GetTypeID();
	outHit.objectChannel = ToObjectChannel(outHit.typeId);
	outHit.response = bestResponse;
	outHit.distance = std::sqrt(best);
	outHit.point = bestCenter; // TODO: SegmentCastHitと同じく、交点取得対応後に正確な衝突点へ置き換える。
	outHit.normal = {}; // TODO: OBB vs Segmentが法線を返せる段階で設定する。
#ifdef _DEBUG
	lastTraceHitResult_ = outHit;
#endif
	return true;
}

bool CollisionManager::RaycastSingle(const RaycastQuery& query, RaycastHit& outHit) const
{
	const std::vector<RaycastHit> hits = RaycastAll(query);
	if (hits.empty())
	{
		outHit = {};
		return false;
	}

	outHit = hits.front();
	return true;
}

std::vector<RaycastHit> CollisionManager::RaycastAll(const RaycastQuery& query) const
{
	// Raycastは問い合わせ専用で、Collision Eventや接触履歴を一切更新しない。
	const RaycastQuery normalizedQuery = NormalizeRaycastQuery(query);
	std::vector<RaycastHit> hits;

	for (K4E::Collider* collider : all_)
	{
		RaycastHit hit{};
		if (RaycastCollider(normalizedQuery, collider, hit))
		{
			hits.push_back(hit);
		}
	}

	std::sort(hits.begin(), hits.end(), [](const RaycastHit& lhs, const RaycastHit& rhs)
		{
			return lhs.distance < rhs.distance;
		});

	RecordRaycastDebugLine(normalizedQuery, hits);
	return hits;
}

RaycastQuery CollisionManager::NormalizeRaycastQuery(const RaycastQuery& query) const
{
	RaycastQuery normalized = query;
	normalized.direction = K4E::Vector3::NormalizeSafe(query.direction, { 0.0f, 0.0f, 1.0f });
	normalized.maxDistance = (std::max)(0.0f, query.maxDistance);
	return normalized;
}

bool CollisionManager::RaycastCollider(const RaycastQuery& query, K4E::Collider* collider, RaycastHit& outHit) const
{
	if (!IsColliderProcessable(collider)) return false;

	const uint32_t objectChannelId = collider->GetObjectChannelId();
	const ECollisionResponse response = traceResponseMatrix_.GetResponse(query.traceChannel, objectChannelId);
	if (response == ECollisionResponse::Ignore) return false;

	float distance = 0.0f;
	K4E::Vector3 normal{};
	if (!RaycastShape(query, collider, distance, normal)) return false;
	if (distance < 0.0f || distance > query.maxDistance) return false;

	outHit.hit = true;
	outHit.collider = collider;
	outHit.typeId = collider->GetTypeID();
	outHit.objectChannel = ToObjectChannel(objectChannelId);
	outHit.response = response;
	outHit.distance = distance;
	outHit.point = query.origin + query.direction * distance;
	outHit.normal = normal;
	return true;
}

bool CollisionManager::RaycastShape(const RaycastQuery& query, K4E::Collider* collider, float& outDistance, K4E::Vector3& outNormal) const
{
	if (!collider) return false;

	switch (collider->GetShapeType())
	{
	case K4E::ECollisionShapeType::Sphere:
		return RaycastSphere(query, collider->GetSphere(), outDistance, outNormal);
	case K4E::ECollisionShapeType::AABB:
		return RaycastAABB(query, collider->GetAABB(), outDistance, outNormal);
	case K4E::ECollisionShapeType::OBB:
		return RaycastOBB(query, collider->GetOBB(), outDistance, outNormal);
	case K4E::ECollisionShapeType::Capsule:
	case K4E::ECollisionShapeType::Segment:
	case K4E::ECollisionShapeType::None:
	default:
		// Capsule/Segment/Mesh系は今回の主目的外なので、まず派生AABBで拾えるようにする。
		return RaycastAABB(query, collider->GetAABB(), outDistance, outNormal);
	}
}

bool CollisionManager::RaycastAABB(const RaycastQuery& query, const K4E::AABB& aabb, float& outDistance, K4E::Vector3& outNormal) const
{
	constexpr float kEpsilon = 1.0e-6f;
	float tMin = 0.0f;
	float tMax = query.maxDistance;
	K4E::Vector3 hitNormal = query.direction * -1.0f;

	const float origin[3] = { query.origin.x, query.origin.y, query.origin.z };
	const float direction[3] = { query.direction.x, query.direction.y, query.direction.z };
	const float minValue[3] = { aabb.min.x, aabb.min.y, aabb.min.z };
	const float maxValue[3] = { aabb.max.x, aabb.max.y, aabb.max.z };
	const K4E::Vector3 axisNormals[3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
	};

	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(direction[axis]) < kEpsilon)
		{
			if (origin[axis] < minValue[axis] || origin[axis] > maxValue[axis])
			{
				return false;
			}
			continue;
		}

		const float invDirection = 1.0f / direction[axis];
		float nearT = (minValue[axis] - origin[axis]) * invDirection;
		float farT = (maxValue[axis] - origin[axis]) * invDirection;
		K4E::Vector3 nearNormal = axisNormals[axis] * -1.0f;
		if (nearT > farT)
		{
			std::swap(nearT, farT);
			nearNormal = axisNormals[axis];
		}

		if (nearT > tMin)
		{
			tMin = nearT;
			hitNormal = nearNormal;
		}
		tMax = (std::min)(tMax, farT);
		if (tMin > tMax)
		{
			return false;
		}
	}

	outDistance = tMin;
	outNormal = K4E::Vector3::NormalizeSafe(hitNormal, query.direction * -1.0f);
	return outDistance <= query.maxDistance;
}

bool CollisionManager::RaycastSphere(const RaycastQuery& query, const K4E::Sphere& sphere, float& outDistance, K4E::Vector3& outNormal) const
{
	const K4E::Vector3 m = query.origin - sphere.center;
	const float b = K4E::Vector3::Dot(m, query.direction);
	const float c = K4E::Vector3::Dot(m, m) - sphere.radius * sphere.radius;

	if (c > 0.0f && b > 0.0f) return false;

	const float discriminant = b * b - c;
	if (discriminant < 0.0f) return false;

	float distance = -b - std::sqrt(discriminant);
	if (distance < 0.0f) distance = 0.0f;
	if (distance > query.maxDistance) return false;

	const K4E::Vector3 point = query.origin + query.direction * distance;
	outDistance = distance;
	outNormal = K4E::Vector3::NormalizeSafe(point - sphere.center, query.direction * -1.0f);
	return true;
}

bool CollisionManager::RaycastOBB(const RaycastQuery& query, const K4E::OBB& obb, float& outDistance, K4E::Vector3& outNormal) const
{
	const K4E::Vector3 relOrigin = query.origin - obb.center;
	const K4E::Vector3 localOrigin{
		K4E::Vector3::Dot(relOrigin, obb.orientations[0]),
		K4E::Vector3::Dot(relOrigin, obb.orientations[1]),
		K4E::Vector3::Dot(relOrigin, obb.orientations[2]),
	};
	const K4E::Vector3 localDirection{
		K4E::Vector3::Dot(query.direction, obb.orientations[0]),
		K4E::Vector3::Dot(query.direction, obb.orientations[1]),
		K4E::Vector3::Dot(query.direction, obb.orientations[2]),
	};

	RaycastQuery localQuery = query;
	localQuery.origin = localOrigin;
	localQuery.direction = K4E::Vector3::NormalizeSafe(localDirection, { 0.0f, 0.0f, 1.0f });

	const K4E::AABB localAabb{ obb.size * -1.0f, obb.size };
	K4E::Vector3 localNormal{};
	if (!RaycastAABB(localQuery, localAabb, outDistance, localNormal)) return false;

	outNormal = K4E::Vector3::NormalizeSafe(
		obb.orientations[0] * localNormal.x +
		obb.orientations[1] * localNormal.y +
		obb.orientations[2] * localNormal.z,
		query.direction * -1.0f);
	return true;
}

void CollisionManager::RecordRaycastDebugLine(const RaycastQuery& query, const std::vector<RaycastHit>& hits) const
{
#ifndef _DEBUG
	(void)query;
	(void)hits;
	return;
#else
	hasLastRaycastQuery_ = true;
	lastRaycastQuery_ = query;
	lastRaycastHits_ = hits;
	raycastDebugLines_.clear();

	if (!K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals()) return;

	const K4E::Vector3 end = query.origin + query.direction * query.maxDistance;
	raycastDebugLines_.push_back({
		query.origin,
		end,
		hits.empty() ? K4E::Vector4{ 0.35f, 0.65f, 1.0f, 1.0f } : K4E::Vector4{ 1.0f, 0.25f, 0.15f, 1.0f },
		!hits.empty(),
		});

	for (const RaycastHit& hit : hits)
	{
		constexpr float kMarkerSize = 0.25f;
		raycastDebugLines_.push_back({
			hit.point - K4E::Vector3{ kMarkerSize, 0.0f, 0.0f },
			hit.point + K4E::Vector3{ kMarkerSize, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 0.15f, 1.0f },
			true,
			});
		raycastDebugLines_.push_back({
			hit.point - K4E::Vector3{ 0.0f, kMarkerSize, 0.0f },
			hit.point + K4E::Vector3{ 0.0f, kMarkerSize, 0.0f },
			{ 1.0f, 1.0f, 0.15f, 1.0f },
			true,
			});
		raycastDebugLines_.push_back({
			hit.point - K4E::Vector3{ 0.0f, 0.0f, kMarkerSize },
			hit.point + K4E::Vector3{ 0.0f, 0.0f, kMarkerSize },
			{ 1.0f, 1.0f, 0.15f, 1.0f },
			true,
			});
	}
#endif
}

ECollisionResponse CollisionManager::GetCollisionResponseForPair(uint32_t selfTypeId, uint32_t otherTypeId) const
{
	// TypeID同士の参照にまとめ、既存TypeIDからObjectChannelへの移行点を一箇所に寄せる。
	return responseMatrix_.GetResponse(selfTypeId, otherTypeId);
}

ECollisionResponse CollisionManager::GetCollisionResponseForCollider(K4E::Collider* self, K4E::Collider* other) const
{
	if (!self || !other) return ECollisionResponse::Ignore;

	const uint32_t selfObjectChannelId = self->GetObjectChannelId();
	const uint32_t otherObjectChannelId = other->GetObjectChannelId();
	if (self->HasCollisionResponseOverrides())
	{
		// Preset適用済みColliderは、自身が相手ObjectChannelをどう扱うかを優先する。
		return static_cast<ECollisionResponse>(self->GetCollisionResponseId(otherObjectChannelId));
	}

	// Preset未適用Colliderは既存ResponseMatrixを使い、従来のCollisionTypeIdDef挙動へフォールバックする。
	return responseMatrix_.GetResponse(selfObjectChannelId, otherObjectChannelId);
}

ECollisionResponse CollisionManager::ResolveCollisionResponseForPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const
{
	const ECollisionResponse responseA = GetCollisionResponseForCollider(colliderA, colliderB);
	const ECollisionResponse responseB = GetCollisionResponseForCollider(colliderB, colliderA);

	// UE風に、片側でもIgnoreならペア全体を判定対象から外す。
	if (responseA == ECollisionResponse::Ignore || responseB == ECollisionResponse::Ignore)
	{
		return ECollisionResponse::Ignore;
	}

	// 片側でもOverlapなら「接触イベントのみ」の扱いにし、将来の押し戻し対象から外せるようにする。
	if (responseA == ECollisionResponse::Overlap || responseB == ECollisionResponse::Overlap)
	{
		return ECollisionResponse::Overlap;
	}

	// Trigger Colliderは形状交差をOverlap通知として扱い、将来の押し戻し候補から外す。
	if ((colliderA && colliderA->IsTrigger()) || (colliderB && colliderB->IsTrigger()))
	{
		return ECollisionResponse::Overlap;
	}

	// 両者がBlockを望む場合だけ、将来の押し戻し候補として扱う。
	return ECollisionResponse::Block;
}

bool CollisionManager::ShouldSkipCollisionPair(ECollisionResponse response) const
{
	// Ignoreは形状判定も接触イベントも不要なため、Narrow Phaseへ入る前に除外する。
	return response == ECollisionResponse::Ignore;
}

bool CollisionManager::IsCollisionIgnored(uint32_t selfTypeId, uint32_t otherTypeId) const
{
	// TypeID固定ペア列挙の早期スキップ用。Collider個別設定はIsCollisionIgnored(Collider*, Collider*)で扱う。
	return ShouldSkipCollisionPair(GetCollisionResponseForPair(selfTypeId, otherTypeId));
}

bool CollisionManager::IsCollisionIgnored(K4E::Collider* colliderA, K4E::Collider* colliderB) const
{
	// Collider単位のPreset Responseを含めたIgnore判定入口。
	return ShouldSkipCollisionPair(ResolveCollisionResponseForPair(colliderA, colliderB));
}

bool CollisionManager::IsColliderProcessable(K4E::Collider* collider) const
{
	// Ownerの生存/表示/有効状態も含めた、Manager側の共通フィルタ。
	if (!collider || !collider->IsCollisionEnabledForQuery()) return false;

	// WorldStaticはステージ由来の所有者なしColliderがあるため、静的Worldとして例外的に許可する。
	const bool isWorldStatic = collider->GetObjectChannelId() == static_cast<uint32_t>(CollisionTypeIdDef::kWorld);
	if (!isWorldStatic && !collider->GetOwner<void>()) return false;
	return true;
}

bool CollisionManager::ShouldShowCollisionDebugViewer() const
{
#ifndef _DEBUG
	return false;
#else
	return K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals();
#endif
}

K4E::Vector4 CollisionManager::GetColliderDebugDrawColor(K4E::Collider* collider) const
{
	if (!collider) return { 0.18f, 0.18f, 0.18f, 1.0f };

	if (!IsColliderProcessable(collider))
	{
		return collider->IsEnabled()
			? K4E::Vector4{ 0.18f, 0.18f, 0.18f, 0.75f }
			: K4E::Vector4{ 0.45f, 0.45f, 0.45f, 0.75f };
	}

	ECollisionResponse currentResponse = ECollisionResponse::Ignore;
	if (TryGetCurrentContactResponse(collider, currentResponse))
	{
		if (currentResponse == ECollisionResponse::Block)
		{
			return { 1.0f, 0.18f, 0.12f, 1.0f };
		}
		if (currentResponse == ECollisionResponse::Overlap)
		{
			return { 1.0f, 0.9f, 0.10f, 1.0f };
		}
		return { 0.18f, 0.18f, 0.18f, 0.75f };
	}

	if (collider->IsTrigger())
	{
		return { 1.0f, 0.9f, 0.10f, 0.9f };
	}

	return { 0.20f, 0.95f, 0.35f, 0.9f };
}

bool CollisionManager::TryGetCurrentContactResponse(K4E::Collider* collider, ECollisionResponse& outResponse) const
{
	if (!collider) return false;

	for (const auto& [_, contact] : currentContacts_)
	{
		if (contact.colliderA == collider || contact.colliderB == collider)
		{
			outResponse = contact.response;
			return true;
		}
	}

	return false;
}

const char* CollisionManager::GetColliderSkipReason(K4E::Collider* collider) const
{
	if (!collider) return "Null collider";
	if (!collider->IsEnabled()) return "Collider disabled";
	if (!collider->IsQueryEnabled()) return "Query disabled";
	if (collider->RequiresOwner() && !collider->GetOwner<void>()) return "Required owner is null";
	if (!collider->IsOwnerActive()) return "Owner inactive flag";
	if (!collider->IsOwnerAlive()) return "Owner dead flag";
	if (!collider->IsOwnerVisible()) return "Owner invisible flag";

	const bool isWorldStatic = collider->GetObjectChannelId() == static_cast<uint32_t>(CollisionTypeIdDef::kWorld);
	if (!isWorldStatic && !collider->GetOwner<void>()) return "Dynamic collider has no owner";
	return "Active";
}

bool CollisionManager::TestCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB) const
{
	// Narrow Phase入口として形状判定だけを担当し、接触状態やイベント通知はここでは更新しない。
	if (!IsColliderProcessable(colliderA) || !IsColliderProcessable(colliderB)) return false;
	const auto key = std::make_pair(colliderA->GetTypeID(), colliderB->GetTypeID());
	const auto it = collisionTable_.find(key);
	if (it == collisionTable_.end()) return false;
	return it->second(colliderA, colliderB);
}

void CollisionManager::UpdateContactState(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response)
{
	// Collider別の接触IDはDebug表示と段階移行用に残し、イベント配送はManagerのペア履歴で行う。
	colliderA->AddCollisionThisFrame(colliderB->GetUniqueID());
	colliderB->AddCollisionThisFrame(colliderA->GetUniqueID());

	const CollisionEventPairKey key = MakeCollisionEventPairKey(colliderA, colliderB);
	currentContacts_[key] = CollisionEventContact{ key, colliderA, colliderB, response };
}

CollisionEventPairKey CollisionManager::MakeCollisionEventPairKey(K4E::Collider* colliderA, K4E::Collider* colliderB) const
{
	const uint32_t idA = colliderA ? colliderA->GetUniqueID() : 0;
	const uint32_t idB = colliderB ? colliderB->GetUniqueID() : 0;
	return {
		idA < idB ? idA : idB,
		idA < idB ? idB : idA,
	};
}

K4E::CollisionHit CollisionManager::BuildCollisionHit(K4E::Collider* self, K4E::Collider* other, ECollisionResponse response) const
{
	K4E::CollisionHit hit{};
	hit.self = self;
	hit.other = other;
	hit.response = response;
	if (self && other)
	{
		hit.point = (self->GetCenterPosition() + other->GetCenterPosition()) * 0.5f;
		const K4E::Vector3 toOther = other->GetCenterPosition() - self->GetCenterPosition();
		const float length = K4E::Vector3::Length(toOther);
		hit.distance = length;
		if (length > 0.0001f)
		{
			hit.normal = toOther * (1.0f / length);
		}
	}
	return hit;
}

void CollisionManager::DispatchCollisionEvents()
{
	// 現在/前回のペア集合を比較し、Enter/Stay/Exitを1ペアにつき1回だけ確定する。
	for (const auto& [key, contact] : currentContacts_)
	{
		const bool wasTouching = previousContacts_.find(key) != previousContacts_.end();
		DispatchActiveCollisionEvent(contact, wasTouching);
	}

	for (const auto& [key, contact] : previousContacts_)
	{
		if (currentContacts_.find(key) != currentContacts_.end()) continue;
		DispatchExitCollisionEvent(contact);
	}
}

void CollisionManager::DispatchActiveCollisionEvent(const CollisionEventContact& contact, bool wasTouching)
{
	K4E::Collider* colliderA = contact.colliderA;
	K4E::Collider* colliderB = contact.colliderB;
	if (!IsColliderProcessable(colliderA) || !IsColliderProcessable(colliderB)) return;

	const K4E::CollisionHit hitA = BuildCollisionHit(colliderA, colliderB, contact.response);
	const K4E::CollisionHit hitB = BuildCollisionHit(colliderB, colliderA, contact.response);

	if (contact.response == ECollisionResponse::Block)
	{
		if (wasTouching)
		{
			++lastCollisionStayEventCount_;
			colliderA->OnCollisionStay(hitA);
			colliderB->OnCollisionStay(hitB);
		}
		else
		{
			++lastCollisionEnterEventCount_;
			colliderA->OnCollisionEnter(hitA);
			colliderB->OnCollisionEnter(hitB);
		}
		return;
	}

	if (contact.response == ECollisionResponse::Overlap)
	{
		if (wasTouching)
		{
			++lastOverlapStayEventCount_;
			colliderA->OnOverlapStay(hitA);
			colliderB->OnOverlapStay(hitB);
		}
		else
		{
			++lastOverlapBeginEventCount_;
			colliderA->OnOverlapBegin(hitA);
			colliderB->OnOverlapBegin(hitB);
		}
	}
}

void CollisionManager::DispatchExitCollisionEvent(const CollisionEventContact& contact)
{
	K4E::Collider* colliderA = contact.colliderA;
	K4E::Collider* colliderB = contact.colliderB;
	if (!IsColliderProcessable(colliderA) || !IsColliderProcessable(colliderB)) return;

	const K4E::CollisionHit hitA = BuildCollisionHit(colliderA, colliderB, contact.response);
	const K4E::CollisionHit hitB = BuildCollisionHit(colliderB, colliderA, contact.response);

	if (contact.response == ECollisionResponse::Block)
	{
		++lastCollisionExitEventCount_;
		colliderA->OnCollisionExit(hitA);
		colliderB->OnCollisionExit(hitB);
		return;
	}

	if (contact.response == ECollisionResponse::Overlap)
	{
		++lastOverlapEndEventCount_;
		colliderA->OnOverlapEnd(hitA);
		colliderB->OnOverlapEnd(hitB);
	}
}

void CollisionManager::ProcessCollisionPairByResponse(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response)
{
	// Block/Overlapの入口だけ分け、現段階では既存イベント互換の処理へ流す。
	switch (response)
	{
	case ECollisionResponse::Block:
		ProcessBlockCollisionPair(colliderA, colliderB);
		break;
	case ECollisionResponse::Overlap:
		ProcessOverlapCollisionPair(colliderA, colliderB);
		break;
	case ECollisionResponse::Ignore:
	default:
		break;
	}
}

std::vector<CollisionBroadPhaseTypePair> CollisionManager::BuildLegacyBroadPhaseTypePairs() const
{
	using CId = uint32_t;
	const CId kPlayer = static_cast<CId>(CollisionTypeIdDef::kPlayer);
	const CId kEnemy = static_cast<CId>(CollisionTypeIdDef::kEnemy);
	const CId kBoss = static_cast<CId>(CollisionTypeIdDef::kBoss);
	const CId kBullet = static_cast<CId>(CollisionTypeIdDef::kBullet);
	const CId kEnemyBullet = static_cast<CId>(CollisionTypeIdDef::kEnemyBullet);
	const CId kBossBullet = static_cast<CId>(CollisionTypeIdDef::kBossBullet);
	const CId kItem = static_cast<CId>(CollisionTypeIdDef::kItem);
	const CId kWorld = static_cast<CId>(CollisionTypeIdDef::kWorld);
	const CId kCrystal = static_cast<CId>(CollisionTypeIdDef::kCrystal);

	// CheckAllCollisionsの固定TypeID列挙と同じ順序を保ち、将来のBroad Phase比較に使う。
	return {
		{ kBoss, kPlayer },
		{ kEnemy, kPlayer },
		{ kBullet, kEnemy },
		{ kBullet, kCrystal },
		{ kBoss, kBullet },
		{ kEnemyBullet, kPlayer },
		{ kPlayer, kBossBullet },
		{ kPlayer, kItem },
		{ kPlayer, kWorld },
		{ kEnemy, kWorld },
		{ kBoss, kWorld },
		{ kBullet, kWorld },
		{ kEnemyBullet, kWorld },
		{ kBossBullet, kWorld },
	};
}

void CollisionManager::CollectPotentialPairsWithBruteForceBroadPhase(std::vector<CollisionPair>& outPairs) const
{
	// ResponseMatrix判定はCollisionManager側に残し、Broad Phaseは候補ペア収集だけを担当する。
	bruteForceBroadPhase_.CollectPairs(buckets_, BuildLegacyBroadPhaseTypePairs(), outPairs);
}

void CollisionManager::CollectPotentialPairsWithUniformGridBroadPhase(std::vector<CollisionPair>& outPairs) const
{
	// UniformGridは試験実装として近傍候補だけを集め、Narrow PhaseとResponseMatrix判定は既存側に残す。
	uniformGridBroadPhase_.CollectPairs(buckets_, BuildLegacyBroadPhaseTypePairs(), outPairs);
}

size_t CollisionManager::CollectPotentialPairsForDebug(std::vector<CollisionPair>& outPairs) const
{
	// Debug確認専用の候補収集入口で、本番CheckAllCollisionsのイベント順には影響させない。
	switch (broadPhaseMode_)
	{
	case ECollisionBroadPhaseMode::UniformGrid:
		CollectPotentialPairsWithUniformGridBroadPhase(outPairs);
		break;
	case ECollisionBroadPhaseMode::BruteForce:
	default:
		CollectPotentialPairsWithBruteForceBroadPhase(outPairs);
		break;
	}

	lastBroadPhaseCandidatePairCount_ = outPairs.size();
	return lastBroadPhaseCandidatePairCount_;
}

CollisionBroadPhaseDebugComparison CollisionManager::CompareBroadPhasesForDebug() const
{
#ifndef _DEBUG
	lastBroadPhaseComparison_ = {};
	return lastBroadPhaseComparison_;
#else
	// Releaseや通常実行へ負荷を載せないよう、比較は呼ばれた時だけ両BroadPhaseを実行する。
	std::vector<CollisionPair> bruteForcePairs;
	std::vector<CollisionPair> uniformGridPairs;
	CollectPotentialPairsWithBruteForceBroadPhase(bruteForcePairs);
	CollectPotentialPairsWithUniformGridBroadPhase(uniformGridPairs);

	std::unordered_set<uint64_t> bruteForceKeys;
	std::unordered_set<uint64_t> uniformGridKeys;
	CountDuplicatePairsForDebug(bruteForcePairs, bruteForceKeys);
	const size_t uniformGridDuplicateCount = CountDuplicatePairsForDebug(uniformGridPairs, uniformGridKeys);

	size_t missingCount = 0;
	for (uint64_t bruteForceKey : bruteForceKeys)
	{
		// BruteForceにある候補がUniformGridにない場合、現段階ではUniformGridを本番利用しない判断材料にする。
		if (uniformGridKeys.find(bruteForceKey) == uniformGridKeys.end())
		{
			++missingCount;
		}
	}

	lastBroadPhaseComparison_.bruteForcePairCount = bruteForcePairs.size();
	lastBroadPhaseComparison_.uniformGridPairCount = uniformGridPairs.size();
	lastBroadPhaseComparison_.uniformGridMissingPairCount = missingCount;
	lastBroadPhaseComparison_.uniformGridDuplicatePairCount = uniformGridDuplicateCount;
	lastBroadPhaseComparison_.uniformGridHasMissingPairs = missingCount > 0;
	return lastBroadPhaseComparison_;
#endif
}

void CollisionManager::ProcessBlockCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// Block接触はOnCollisionEnter/Stay/Exitへ配送する。押し戻し連携はまだ行わない。
	CheckCollisionPair(colliderA, colliderB, ECollisionResponse::Block);
}

void CollisionManager::ProcessOverlapCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB)
{
	// Overlap/Trigger接触はOnOverlapBegin/Stay/Endへ配送する。
	CheckCollisionPair(colliderA, colliderB, ECollisionResponse::Overlap);
}

/// -------------------------------------------------------------
///                 コライダー２つの衝突判定と接触登録
/// -------------------------------------------------------------
void CollisionManager::CheckCollisionPair(K4E::Collider* colliderA, K4E::Collider* colliderB, ECollisionResponse response)
{
	// Broad Phase導入後も、この関数以降は既存イベント順を守るNarrow Phase互換入口として残す。
	if (!IsColliderProcessable(colliderA) || !IsColliderProcessable(colliderB)) return;

	// 自分同士は無視
	if (colliderA == colliderB) return;

	// Preset/Matrixの最終ResponseがIgnoreなら、形状判定もイベント登録も行わない。
	if (ShouldSkipCollisionPair(response)) return;

	// 登録済み形状判定で交差したペアだけを、このフレームの接触として扱う。
	if (!TestCollisionPair(colliderA, colliderB)) return;

	// 衝突していたので「このフレーム接触中」を両者へ登録
	UpdateContactState(colliderA, colliderB, response);
}

/// -------------------------------------------------------------
///                 コライダーの衝突判定関数の登録
/// -------------------------------------------------------------
void CollisionManager::RegisterCollisionFuncsions()
{
	using CollisionType = uint32_t;
	constexpr CollisionType kPlayer = static_cast<CollisionType>(CollisionTypeIdDef::kPlayer);
	constexpr CollisionType kEnemy = static_cast<CollisionType>(CollisionTypeIdDef::kEnemy);
	constexpr CollisionType kBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBullet);
	constexpr CollisionType kEnemyBullet = static_cast<CollisionType>(CollisionTypeIdDef::kEnemyBullet);
	constexpr CollisionType kBossBullet = static_cast<CollisionType>(CollisionTypeIdDef::kBossBullet);
	constexpr CollisionType kItem = static_cast<CollisionType>(CollisionTypeIdDef::kItem);
	constexpr CollisionType kBoss = static_cast<CollisionType>(CollisionTypeIdDef::kBoss);
	constexpr CollisionType kWorld = static_cast<CollisionType>(CollisionTypeIdDef::kWorld);
	constexpr CollisionType kCrystal = static_cast<CollisionType>(CollisionTypeIdDef::kCrystal);

	auto AddCollisionFunc = [&](CollisionType a, CollisionType b, const CollisionFunc& func) {
		collisionTable_[{a, b}] = func;
		};

	auto AddSymmetricCollisionFunc = [&](CollisionType a, CollisionType b, const CollisionFunc& func) {
		AddCollisionFunc(a, b, func);
		AddCollisionFunc(b, a, func);
		};

	// OBB vs OBB
	const CollisionFunc OBB_OBB = [](K4E::Collider* a, K4E::Collider* b) {
		return K4E::CollisionPrimitiveTests::TestOBBOBB(a->GetOBB(), b->GetOBB());
		};

	// Segment vs OBB（弾など）
	const CollisionFunc SEG_OBB = [](K4E::Collider* segOwner, K4E::Collider* obbOwner) {
		return K4E::CollisionPrimitiveTests::TestOBBSegment(obbOwner->GetOBB(), segOwner->GetSegment());
		};

	// OBB vs OBB（左右対称）
	for (auto [a, b] : std::initializer_list<std::pair<CollisionType, CollisionType>>{
		{ kPlayer, kEnemy },
		{ kPlayer, kBoss },
		{ kPlayer, kItem },
		{ kPlayer, kWorld },
		})
	{
		AddSymmetricCollisionFunc(a, b, OBB_OBB);
	}

	// Segment vs OBB（左右対称）
	//  - 弾は Segment、キャラ/ワールドは OBB として扱う
	for (auto [seg, obb] : std::initializer_list<std::pair<CollisionType, CollisionType>>{
		{ kBullet, kEnemy },
		{ kBullet, kCrystal },
		{ kBullet, kBoss },
		{ kBullet, kWorld },
		{ kEnemyBullet, kPlayer },
		{ kEnemyBullet, kWorld },
		{ kBossBullet, kPlayer },
		{ kBossBullet, kWorld },
		})
	{
		AddCollisionFunc(seg, obb, SEG_OBB);
		AddCollisionFunc(obb, seg, [=](K4E::Collider* obbOwner, K4E::Collider* segOwner) {
			return SEG_OBB(segOwner, obbOwner);
			});
	}
}
