#include "ColliderComponent.h"
#include <Actor.h>
#include <SceneComponent.h>

#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	class ColliderComponent::ForwardingCollider final : public Collider
	{
	public:
		explicit ForwardingCollider(ColliderComponent* owner) : owner_(owner) {}

		void OnCollision(Collider* other) override
		{
			if (owner_) owner_->DispatchCollision(other);
		}

		void OnCollisionEnter(Collider* other) override
		{
			if (owner_) owner_->DispatchCollisionEnter(other);
		}

		void OnCollisionStay(Collider* other) override
		{
			if (owner_) owner_->DispatchCollisionStay(other);
		}

		void OnCollisionExit(Collider* other) override
		{
			if (owner_) owner_->DispatchCollisionExit(other);
		}

		void OnCollisionEnter(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchCollisionEnter(hit);
		}

		void OnCollisionStay(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchCollisionStay(hit);
		}

		void OnCollisionExit(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchCollisionExit(hit);
		}

		void OnOverlapBegin(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchOverlapBegin(hit);
		}

		void OnOverlapStay(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchOverlapStay(hit);
		}

		void OnOverlapEnd(const CollisionHit& hit) override
		{
			if (owner_) owner_->DispatchOverlapEnd(hit);
		}

	private:
		ColliderComponent* owner_ = nullptr;
	};

	namespace
	{
		/// <summary>
		/// Collision形状をJSON保存用文字列へ変換する。
		/// </summary>
		const char* Tostring(ECollisionShapeType shapeType)
		{
			switch (shapeType)
			{
			case Ken4lowEngine::ECollisionShapeType::Sphere:
				return "Sphere";
			case Ken4lowEngine::ECollisionShapeType::AABB:
				return "AABB";
			case Ken4lowEngine::ECollisionShapeType::OBB:
				return "OBB";
			case Ken4lowEngine::ECollisionShapeType::Capsule:
				return "Capsule";
			case Ken4lowEngine::ECollisionShapeType::Segment:
				return "Segment";
			case Ken4lowEngine::ECollisionShapeType::None:
			default:
				return "None";
			}
		}

		/// <summary>
		/// JSON文字列からCollision形状を変換する
		/// </summary>
		ECollisionShapeType ShapeTypeFromString(const std::string& shapeType)
		{
			if (shapeType == "Sphere")
			{
				return ECollisionShapeType::Sphere;
			}
			else if (shapeType == "AABB")
			{
				return ECollisionShapeType::AABB;
			}
			else if (shapeType == "OBB")
			{
				return ECollisionShapeType::OBB;
			}
			else if (shapeType == "Capsule")
			{
				return ECollisionShapeType::Capsule;
			}
			else if (shapeType == "Segment")
			{
				return ECollisionShapeType::Segment;
			}

			return ECollisionShapeType::None;
		}
	}

	void ColliderComponent::Initialize()
	{
		SceneComponent::Initialize(); // 親子関係を考慮したWorldTransformを初期計算する

		collider_ = std::make_unique<ForwardingCollider>(this); // Collision通知をComponentへ戻すColliderを生成する
		collider_->Initialize();
		collider_->SetDebugName(GetName());
		collider_->SetCollisionLayer(collisionLayer_);
		collider_->SetTrigger(isTrigger_);
		if (Actor* owner = GetOwner()) collider_->SetOwner(owner); // Legacy CollisionManagerからも実際のActorをOwnerとして取得できるようにする

		SyncFromSceneTransform();
	}

	void ColliderComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime); // SceneComponent側のTransformを更新する。
		SyncFromSceneTransform();
	}

	void ColliderComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		SyncFromSceneTransform(); // Editorでは物理を進めず、Debug形状の位置だけを追従させる。
	}

	void ColliderComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!collider_)
		{
			return; // Colliderが生成されていない場合は同期しない
		}

		Actor* owner = GetOwner();
		if (!owner)
		{
			return; // 所有者Actorが存在しない場合は同期しない
		}

		SceneComponent* root = owner->GetRootComponent();
		if (!root)
		{
			return; // 所有者ActorのRootComponentが存在しない場合は同期しない
		}

		const Vector3 currentWorldPosition = GetWorldPosition();
		const Vector3 correctedWorldPosition = collider_->GetCenterPosition();
		const Vector3 correctionDelta = correctedWorldPosition - currentWorldPosition;

		constexpr float kCorrectionEpsilon = 0.0001f; // 許容誤差の閾値
		if (Vector3::Length(correctionDelta) <= kCorrectionEpsilon)
		{
			return; // 補正差分がほぼない場合はTransformを動かさない
		}

		if (root == this)
		{
			LocalPosition() += correctionDelta; // RootComponentの場合はLocalPositionを直接修正する
		}
		else
		{
			root->LocalPosition() += correctionDelta; // RootComponent以外の場合はRootComponentのLocalPositionを修正する
		}

		root->RefreshWorldTransform(); // RootComponentのWorldTransformを更新する
		SyncFromSceneTransform();      // SceneComponentのTransformをColliderへ再同期する
	}

	void ColliderComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // ColliderのTransform情報を確認・編集できるようにする。

#ifdef USE_IMGUI
		ImGui::SeparatorText("Collider Component");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif
	}

	void ColliderComponent::Finalize()
	{
		ClearCollisionCallbacks(); // Actor再構築後に古い所有者へ通知しないよう外部参照を解除する
		collider_.reset();
	}

	void ColliderComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<ColliderComponent*>(this)->CreateProperties(), outJson);
	}

	void ColliderComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		if (inJson.contains("CollisionLayer") && inJson["CollisionLayer"].is_number_unsigned())
		{
			SetCollisionLayer(inJson["CollisionLayer"].get<uint32_t>());
		}
	}

	void ColliderComponent::SetRadius(float radius)
	{
		radius_ = std::clamp(radius, 0.0f, 100.0f);
		SyncFromSceneTransform();
	}

	void ColliderComponent::SetShapeType(ECollisionShapeType shapeType)
	{
		shapeType_ = shapeType;
		SyncFromSceneTransform();
	}

	void ColliderComponent::SetHalfSize(const Vector3& halfSize)
	{
		halfSize_ = {
			std::clamp(halfSize.x, 0.0f, 100.0f),
			std::clamp(halfSize.y, 0.0f, 100.0f),
			std::clamp(halfSize.z, 0.0f, 100.0f)
		};
		SyncFromSceneTransform();
	}

	void ColliderComponent::SetCollisionLayer(uint32_t layer)
	{
		collisionLayer_ = layer; // ColliderComponentの衝突レイヤーを設定する

		if (collider_)
		{
			collider_->SetCollisionLayer(collisionLayer_); // Colliderの衝突レイヤーを設定する
		}
	}

	void ColliderComponent::SetCollisionLayer(PhysicsCollisionLayer layer)
	{
		// 名前付きレイヤーを既存の数値レイヤーへ変換して設定する
		SetCollisionLayer(static_cast<uint32_t>(layer));
	}

	void ColliderComponent::ClearCollisionCallbacks()
	{
		onCollision_ = {};
		onCollisionEnter_ = {};
		onCollisionStay_ = {};
		onCollisionExit_ = {};
		onCollisionEnterHit_ = {};
		onCollisionStayHit_ = {};
		onCollisionExitHit_ = {};
		onOverlapBegin_ = {};
		onOverlapStay_ = {};
		onOverlapEnd_ = {};
	}

	void ColliderComponent::DispatchCollision(Collider* other)
	{
		if (onCollision_) onCollision_(other);
	}

	void ColliderComponent::DispatchCollisionEnter(Collider* other)
	{
		if (onCollisionEnter_) onCollisionEnter_(other);
		else DispatchCollision(other);
	}

	void ColliderComponent::DispatchCollisionStay(Collider* other)
	{
		if (onCollisionStay_) onCollisionStay_(other);
		else DispatchCollision(other);
	}

	void ColliderComponent::DispatchCollisionExit(Collider* other)
	{
		if (onCollisionExit_) onCollisionExit_(other);
	}

	void ColliderComponent::DispatchCollisionEnter(const CollisionHit& hit)
	{
		if (onCollisionEnterHit_) onCollisionEnterHit_(hit);
		else DispatchCollisionEnter(hit.other);
	}

	void ColliderComponent::DispatchCollisionStay(const CollisionHit& hit)
	{
		if (onCollisionStayHit_) onCollisionStayHit_(hit);
		else DispatchCollisionStay(hit.other);
	}

	void ColliderComponent::DispatchCollisionExit(const CollisionHit& hit)
	{
		if (onCollisionExitHit_) onCollisionExitHit_(hit);
		else DispatchCollisionExit(hit.other);
	}

	void ColliderComponent::DispatchOverlapBegin(const CollisionHit& hit)
	{
		if (onOverlapBegin_) onOverlapBegin_(hit);
		else DispatchCollisionEnter(hit);
	}

	void ColliderComponent::DispatchOverlapStay(const CollisionHit& hit)
	{
		if (onOverlapStay_) onOverlapStay_(hit);
		else DispatchCollisionStay(hit);
	}

	void ColliderComponent::DispatchOverlapEnd(const CollisionHit& hit)
	{
		if (onOverlapEnd_) onOverlapEnd_(hit);
		else DispatchCollisionExit(hit);
	}

	void ColliderComponent::SyncFromSceneTransform()
	{
		if (!collider_)
		{
			return; // Colliderが生成されていない場合は同期しない
		}

		switch (shapeType_)
		{
		case Ken4lowEngine::ECollisionShapeType::None:
			break;
		case Ken4lowEngine::ECollisionShapeType::Sphere:
			{
				Sphere sphere{};
				sphere.center = GetWorldPosition(); // SceneComponentのWorld位置をColliderの中心に反映する
				sphere.radius = radius_;            // 設定された半径をColliderに反映する
				collider_->SetSphere(sphere);        // ColliderにSphere情報を設定する
				break;
			}
		case Ken4lowEngine::ECollisionShapeType::AABB:
			{
				Vector3 center = GetWorldPosition(); // SceneComponentのWorld位置をColliderの中心に反映する

				AABB aabb{};
				aabb.min = center - halfSize_;
				aabb.max = center + halfSize_;
				collider_->SetAABB(aabb);
				break;
			}
		case Ken4lowEngine::ECollisionShapeType::OBB:
			{
				collider_->SetCenterPosition(GetWorldPosition()); // SceneComponentのWorld位置をColliderの中心に反映する
				collider_->SetOBBHalfSize(halfSize_);
				collider_->SetOrientation(GetWorldRotation()); // SceneComponentのWorld回転をColliderの回転に反映する
				break;
			}
		case Ken4lowEngine::ECollisionShapeType::Capsule:
			break;
		case Ken4lowEngine::ECollisionShapeType::Segment:
			break;
		default:
			break;
		}
	}

	std::vector<ComponentProperty> ColliderComponent::CreateProperties()
	{
		return {
			{ "ShapeType", "形状", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return std::string(Tostring(shapeType_)); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetShapeType(ShapeTypeFromString(*typedValue)); } }, 0.0f, 0.0f, 0.1f, false, { { "None", "None" }, { "Sphere", "Sphere" }, { "AABB", "AABB" }, { "OBB", "OBB" }, { "Capsule", "Capsule" }, { "Segment", "Segment" } } },
			{ "Radius", "半径", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return radius_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetRadius(*typedValue); } }, 0.0f, 100.0f, 0.1f, true },
			{ "HalfSize", "半サイズ", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return halfSize_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector3>(&value)) { SetHalfSize(*typedValue); } }, 0.0f, 100.0f, 0.1f, true },
			{ "IsTrigger", "トリガー", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return isTrigger_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetIsTrigger(*typedValue); } } },
			{ "CollisionLayer", "Collision Layer", ComponentPropertyType::Int, [this]() -> ComponentPropertyValue { return static_cast<int>(collisionLayer_); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<int>(&value)) { SetCollisionLayer(static_cast<uint32_t>(std::max(*typedValue, 0))); } }, 0.0f, 31.0f, 1.0f, true }
		};
	}
} // namespace Ken4lowEngine
