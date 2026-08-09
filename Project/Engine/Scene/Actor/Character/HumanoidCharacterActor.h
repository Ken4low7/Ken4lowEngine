#pragma once

#include "CharacterActor.h"
#include "CharacterColliderComponent.h"
#include "HumanoidVisualComponent.h"
#include <Collider.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// Character共通ComponentとHumanoidVisualComponentだけで人型キャラクターを構成するActor。
	class HumanoidCharacterActor : public CharacterActor, public Collider
	{
	public:
		using BodyPart = HumanoidVisualComponent::BodyPart;

		/// 旧Gameplayの子部位インデックスと同じ順序を維持する。
		struct PartIndices
		{
			uint32_t head = 0;
			uint32_t leftArm = 1;
			uint32_t rightArm = 2;
			uint32_t leftLeg = 3;
			uint32_t rightLeg = 4;
		};

		HumanoidCharacterActor()
			: body_(EnsureVisualComponentForConstruction().GetBodyPart())
			, parts_(EnsureVisualComponentForConstruction().GetParts())
		{
		}

		/// Character共通Componentを生成した後、人型表示ComponentをRootへ接続する。
		void Initialize() override
		{
			CharacterActor::Initialize();
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			if (!visual) return;
			visual->SetName("Humanoid Visual");
			visual->SetUpdateOrder(-40);
			visual->SetDrawOrder(0);
			visual->SetCastShadowEnabled(true);
			if (SceneComponent* root = GetRootComponent()) visual->AttachTo(root);
			visual->InitializeForWorld();
			SyncRootToGameplayBody(); // 初期化直後から旧GameplayがBodyをワールドTransformとして参照できるようにする。
		}

		/// 旧GameplayがBody Transformへ書いた結果をRootへ集約し、Component更新後に同じBody参照へワールド値を戻す。
		void Update(float deltaTime) override
		{
			if (IsDead() && !body_.id.empty() && body_.transform.parent_ == nullptr)
			{
				return; // 崩壊演出でワールド空間へ切り離した部位を通常Humanoid階層へ再接続しない。
			}

			SyncGameplayTransformToRoot();
			CharacterActor::Update(deltaTime);
			SyncRootToGameplayBody();
		}

		/// Actor描画とCollider Debug描画の名前衝突を避け、通常描画はComponent経路だけへ統一する。
		void Draw() override
		{
			CharacterActor::Draw();
		}

		/// Shadow描画もActor/Component経路へ統一する。
		void DrawShadow() override
		{
			CharacterActor::DrawShadow();
		}

		/// Details/ImGui描画はActor側のComponent表示を使用する。
		void DrawImGui() override
		{
			CharacterActor::DrawImGui();
		}

		/// Physics補正後もRootの最終位置を旧GameplayのBody参照へ戻す。
		void PostPhysicsUpdate(float deltaTime) override
		{
			CharacterActor::PostPhysicsUpdate(deltaTime);
			SyncRootToGameplayBody();
		}

		std::string GetClassTypeName() const override { return "HumanoidCharacterActor"; }

		HumanoidVisualComponent* GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }

		BodyPart& GetBody() { return body_; }
		const BodyPart& GetBody() const { return body_; }

		std::vector<BodyPart>& GetBodyParts()
		{
			return parts_; // 実体はHumanoidVisualComponentだけが所有し、旧Gameplayには同じ配列への参照を返す。
		}

		const std::vector<BodyPart>& GetBodyParts() const
		{
			return parts_;
		}

		PartIndices& GetPartIndices() { return partIndices_; }
		const PartIndices& GetPartIndices() const { return partIndices_; }

		WorldTransformEx* GetWorldTransform() { return &body_.transform; }
		const WorldTransformEx* GetWorldTransform() const { return &body_.transform; }

		Vector3 GetCenterPosition() const override
		{
			if (const SceneComponent* root = GetRootComponent()) return root->GetWorldPosition();
			return body_.transform.translate_;
		}

		void SetCenterPosition(const Vector3& position) override
		{
			body_.transform.translate_ = position;
			if (SceneComponent* root = GetRootComponent())
			{
				root->SetLocalPosition(position);
				root->RefreshWorldTransform();
			}
			Collider::SetCenterPosition(position);
		}

		Vector3 GetOBBHalfSize() const override
		{
			const CharacterColliderComponent* component = GetColliderComponent();
			return component ? component->GetHalfSize() : Collider::GetOBBHalfSize();
		}

		void SetOBBHalfSize(const Vector3& halfSize) override
		{
			if (CharacterColliderComponent* component = GetColliderComponent()) component->SetHalfSize(halfSize);
			Collider::SetOBBHalfSize(halfSize);
		}

		Vector3 GetOrientation() const override
		{
			if (const SceneComponent* root = GetRootComponent()) return root->GetWorldRotation();
			return Collider::GetOrientation();
		}

		void SetOrientation(const Vector3& rotation) override
		{
			body_.transform.rotate_ = rotation;
			if (SceneComponent* root = GetRootComponent())
			{
				root->SetLocalRotation(rotation);
				root->RefreshWorldTransform();
			}
			Collider::SetOrientation(rotation);
		}

		void SetBodyActive(bool active)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->SetPartVisible("Body", active);
		}

		void SetAllPartsActive(bool active)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->SetAllPartsVisible(active);
		}

		void SetPartActive(size_t index, bool active)
		{
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			if (!visual || index >= visual->GetParts().size()) return;
			visual->SetPartVisible(visual->GetParts()[index].id, active);
		}

		void ApplySkinToAllParts(const std::string& texturePath)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->ApplySkinToAllParts(texturePath);
		}

		static void ApplySkinTo(Object3D* object, const std::string& texturePath)
		{
			if (object) object->SetTextureForAll(texturePath);
		}

		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
		{
			if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->UpdateShadowMatrices(lightViewProjection);
		}

	protected:
		/// Guardian固有の旧調整値取得は既存データ互換のため残すが、個別部位生成には使用しない。
		virtual Vector3 GetInitialBodyScale() const { return { 1.0f, 1.0f, 1.0f }; }
		virtual Vector3 GetHeadScale() const { return { 1.0f, 1.0f, 1.0f }; }
		virtual Vector3 GetArmScale() const { return { 1.0f, 1.0f, 1.0f }; }
		virtual Vector3 GetLegScale() const { return { 1.0f, 1.0f, 1.0f }; }
		virtual std::string GetBodyModelPath() const { return "Characters/body.gltf"; }
		virtual std::string GetHeadModelPath() const { return "Characters/head.gltf"; }
		virtual std::string GetLeftArmModelPath() const { return "Characters/left_arm.gltf"; }
		virtual std::string GetRightArmModelPath() const { return "Characters/right_arm.gltf"; }
		virtual std::string GetLeftLegModelPath() const { return "Characters/left_leg.gltf"; }
		virtual std::string GetRightLegModelPath() const { return "Characters/right_leg.gltf"; }

		BodyPart& body_; // 旧GameplayもHumanoidVisualComponent所有のBody実体を直接参照し、重複部位を持たない。
		std::vector<BodyPart>& parts_; // 旧Gameplay互換名もHumanoidVisualComponent所有配列への参照だけを保持する。

	private:
		HumanoidVisualComponent& EnsureVisualComponentForConstruction()
		{
			if (HumanoidVisualComponent* existing = GetCharacterComponent<HumanoidVisualComponent>()) return *existing;
			auto& visual = AddComponent<HumanoidVisualComponent>();
			visual.SetName("Humanoid Visual");
			return visual;
		}

		void SyncGameplayTransformToRoot()
		{
			SceneComponent* root = GetRootComponent();
			HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
			if (!root || !visual || body_.id.empty()) return;

			const HumanoidPartDefinition* bodyDefinition = visual->GetDefinition().FindPart("Body");
			const Vector3 localBodyPosition = bodyDefinition ? bodyDefinition->localPosition : Vector3{};
			const Vector3 localBodyRotation = bodyDefinition ? bodyDefinition->localRotation : Vector3{};
			const Vector3 localBodyScale = bodyDefinition ? bodyDefinition->localScale : Vector3{ 1.0f, 1.0f, 1.0f };

			root->SetLocalPosition(body_.transform.translate_ - localBodyPosition);
			root->SetLocalRotation(body_.transform.rotate_ - localBodyRotation);
			root->SetLocalScale(body_.transform.scale_);
			root->RefreshWorldTransform();

			// HumanoidVisualComponentが描画行列を作る間だけBodyを定義ローカル姿勢へ戻す。
			body_.transform.translate_ = localBodyPosition;
			body_.transform.rotate_ = localBodyRotation;
			body_.transform.scale_ = localBodyScale;
		}

		void SyncRootToGameplayBody()
		{
			const SceneComponent* root = GetRootComponent();
			if (!root || body_.id.empty()) return;
			body_.transform.translate_ = root->GetWorldPosition();
			body_.transform.rotate_ = root->GetWorldRotation();
			body_.transform.scale_ = root->GetWorldScale();
			body_.transform.worldTranslate_ = root->GetWorldPosition();
			body_.transform.worldRotate_ = root->GetWorldRotation();
			body_.transform.parent_ = nullptr; // 旧Gameplayへ返すBodyは常にワールドTransformとして保持し、死亡時の再UpdateでRootを二重適用しない。
			Collider::SetCenterPosition(root->GetWorldPosition());
		}

		PartIndices partIndices_{};
	};
} // namespace Ken4lowEngine
