#define NOMINMAX
#include "Collider.h"
#include <Wireframe.h>

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///					　	OBBを取得
	/// -------------------------------------------------------------
	OBB Collider::GetOBB() const
	{
		return shapeInfo_.BuildOBB(); // 既存OBB生成をCollisionShapeInfoへ委譲する。
	}

	void Collider::SetOBBBasis(const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ)
	{
		shapeInfo_.obbBasis[0] = Vector3::Normalize(axisX);
		shapeInfo_.obbBasis[1] = Vector3::Normalize(axisY);
		shapeInfo_.obbBasis[2] = Vector3::Normalize(axisZ);
		shapeInfo_.useOBBBasis = true;
	}

	void Collider::ClearOBBBasis()
	{
		shapeInfo_.useOBBBasis = false;
	}

	/// -------------------------------------------------------------
	///					  衝突状態フレーム開始
	/// -------------------------------------------------------------
	void Collider::BeginCollisionFrame()
	{
		eventState_.BeginFrame(); // 接触履歴更新をCollisionEventStateへ委譲する。
	}

	/// -------------------------------------------------------------
	///					  このフレームでの接触登録
	/// -------------------------------------------------------------
	void Collider::AddCollisionThisFrame(uint32_t otherUniqueId)
	{
		eventState_.AddContact(otherUniqueId); // 接触登録をCollisionEventStateへ委譲する。
	}

	/// -------------------------------------------------------------
	///					　	初期化処理
	/// -------------------------------------------------------------
	void Collider::Initialize()
	{

	}

	/// -------------------------------------------------------------
	///					　	 更新処理
	/// -------------------------------------------------------------
	void Collider::Update()
	{

	}

	/// -------------------------------------------------------------
	///					　	 描画処理
	/// -------------------------------------------------------------
	void Collider::Draw()
	{
		const Vector4 drawColor = IsCollisionEnabledForQuery()
			? shapeInfo_.debugColor
			: Vector4{ 0.35f, 0.35f, 0.35f, 0.65f };
		DrawDebug(drawColor, false);
	}

	void Collider::DrawDebug(const Vector4& drawColor, bool drawBounds)
	{
		// 主形状のDebug描画。未移行Colliderは従来通りOBB/Segmentも下の互換描画で表示する。
		switch (shapeInfo_.shapeType)
		{
		case ECollisionShapeType::Sphere:
			if (shapeInfo_.useSphere && shapeInfo_.sphere.radius > CollisionShapeInfo::kDrawEpsilon)
			{
				Wireframe::GetInstance()->DrawSphere(shapeInfo_.sphere.center, shapeInfo_.sphere.radius, drawColor);
			}
			break;
		case ECollisionShapeType::AABB:
			if (shapeInfo_.HasDrawableOBB())
			{
				Wireframe::GetInstance()->DrawAABB(shapeInfo_.BuildAABB(), drawColor);
			}
			break;
		case ECollisionShapeType::Capsule:
			if ((drawBounds && shapeInfo_.useCapsule && shapeInfo_.capsule.radius > CollisionShapeInfo::kDrawEpsilon) || shapeInfo_.HasDrawableCapsule())
			{
				Wireframe::GetInstance()->DrawCapsule(shapeInfo_.capsule, drawColor);
			}
			break;
		case ECollisionShapeType::Segment:
			if (shapeInfo_.HasDrawableSegment())
			{
				Wireframe::GetInstance()->DrawSegment(shapeInfo_.segment, drawColor);
			}
			break;
		case ECollisionShapeType::OBB:
		default:
			if (shapeInfo_.HasDrawableOBB())
			{
				Wireframe::GetInstance()->DrawOBB(GetOBB(), drawColor);
			}
			break;
		}

		// 半サイズが 0 に非常に近いなら OBB を描画しない（≒ 未設定とみなす）
		if (shapeInfo_.shapeType != ECollisionShapeType::AABB && shapeInfo_.shapeType != ECollisionShapeType::OBB && shapeInfo_.HasDrawableOBB())
		{
			const OBB obb = GetOBB();
			Wireframe::GetInstance()->DrawOBB(obb, drawColor);
		}

		// 線分の長さが十分なら描画（セグメントが有効なら）
		if (shapeInfo_.shapeType != ECollisionShapeType::Segment && shapeInfo_.HasDrawableSegment())
		{
			Wireframe::GetInstance()->DrawSegment(shapeInfo_.segment, drawColor);
		}

		// -------- Capsule 描画 -------- //
		Vector3 axis = shapeInfo_.capsule.GetAxis();
		if (shapeInfo_.shapeType != ECollisionShapeType::Capsule &&
			((drawBounds && shapeInfo_.useCapsule && shapeInfo_.capsule.radius > CollisionShapeInfo::kDrawEpsilon) || shapeInfo_.HasDrawableCapsule()))
		{
			if (Vector3::Length(axis) < 1e-6f)
				Wireframe::GetInstance()->DrawSphere(shapeInfo_.capsule.segment.origin, shapeInfo_.capsule.radius, drawColor);
			else
				Wireframe::GetInstance()->DrawCapsule(shapeInfo_.capsule.GetCenter(), shapeInfo_.capsule.radius, shapeInfo_.capsule.GetHeight(), axis, 8, drawColor);
		}

		if (drawBounds)
		{
			// 中心点と派生AABBを薄く重ね、位置/サイズの取り違えをDebug Viewerで確認しやすくする。
			const Vector4 centerColor{ 0.85f, 1.0f, 1.0f, 1.0f };
			const Vector4 boundsColor{ drawColor.x * 0.55f, drawColor.y * 0.55f, drawColor.z * 0.55f, 0.55f };
			Wireframe::GetInstance()->DrawSphere(GetCenterPosition(), 0.12f, centerColor);
			if (shapeInfo_.HasDrawableOBB())
			{
				Wireframe::GetInstance()->DrawAABB(shapeInfo_.BuildAABB(), boundsColor);
			}
		}
	}

	/// -------------------------------------------------------------
	///					　	 ImGui描画処理
	/// -------------------------------------------------------------
	void Collider::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::TreeNode("Collider")) {
			bool enabled = IsEnabled();
			if (ImGui::Checkbox("Enabled", &enabled)) {
				SetEnabled(enabled);
			}
			bool trigger = IsTrigger();
			if (ImGui::Checkbox("Trigger", &trigger)) {
				SetTrigger(trigger);
			}
			const std::string_view presetName = GetCollisionPresetName();
			if (presetName.empty())
			{
				ImGui::Text("Preset: (manual/unset)");
			}
			else
			{
				ImGui::Text("Preset: %.*s", static_cast<int>(presetName.size()), presetName.data());
			}
			ImGui::Text("TypeID/ObjectChannel: %u / %u", GetTypeID(), GetObjectChannelId());
			ImGui::Text("Query/Physics: %s / %s", IsQueryEnabled() ? "true" : "false", IsPhysicsEnabled() ? "true" : "false");
			ImGui::Text("Owner: %p Required:%s Active:%s Alive:%s Visible:%s",
				GetOwner<void>(),
				RequiresOwner() ? "true" : "false",
				IsOwnerActive() ? "true" : "false",
				IsOwnerAlive() ? "true" : "false",
				IsOwnerVisible() ? "true" : "false");

			Vector3 pos = shapeInfo_.colliderPosition;
			if (ImGui::DragFloat3("Center", &pos.x, 0.1f)) {
				SetCenterPosition(pos);
			}

			Vector3 size = shapeInfo_.colliderHalfSize;
			if (ImGui::DragFloat3("HalfSize", &size.x, 0.1f)) {
				SetOBBHalfSize(size);
			}

			Vector3 rot = shapeInfo_.orientation;
			if (ImGui::DragFloat3("Orientation", &rot.x, 0.1f)) {
				SetOrientation(rot);
			}

			ImGui::ColorEdit4("Color", &shapeInfo_.debugColor.x);

			// -------- Capsule -------- //
			if (ImGui::Checkbox("Use Capsule", &shapeInfo_.useCapsule)) {}
			if (ImGui::Checkbox("Draw Capsule", &shapeInfo_.drawCapsule)) {}
			if (shapeInfo_.useCapsule)
			{
				// Segment::diff は「終点への差分(end - origin)」として扱う
				Vector3 pA = shapeInfo_.capsule.segment.origin;
				Vector3 pB = shapeInfo_.capsule.segment.origin + shapeInfo_.capsule.segment.diff;
				float   r = shapeInfo_.capsule.radius;

				bool changedA = ImGui::DragFloat3("Point A", &pA.x, 0.05f);
				bool changedB = ImGui::DragFloat3("Point B", &pB.x, 0.05f);

				if (changedA)
				{
					// Aを動かしたときはBを固定したいので diff を更新
					shapeInfo_.capsule.segment.origin = pA;
					shapeInfo_.capsule.segment.diff = pB - pA;
				}
				if (changedB && !changedA)
				{
					// Bのみ変更された場合
					shapeInfo_.capsule.segment.diff = pB - shapeInfo_.capsule.segment.origin;
				}

				if (ImGui::DragFloat("Radius", &r, 0.01f))
				{
					shapeInfo_.capsule.radius = std::max(0.0f, r);
				}
			}

			ImGui::TreePop();
		}
#endif // USE_IMGUI
	}

} // namespace Ken4lowEngine
