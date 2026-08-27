#include "RigidbodyComponent.h"
#include "Actor.h"
#include "SceneComponent.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Rigidbodyの種類をJSON保存用文字列へ変換する。
		/// </summary>
		const char* ToString(BodyType bodyType)
		{
			switch (bodyType)
			{
			case BodyType::Static:
				return "Static";
			case BodyType::Dynamic:
				return "Dynamic";
			case BodyType::Kinematic:
				return "Kinematic";
			default:
				return "Unknown";
			}
		}

		BodyType BodyTypeFromString(const std::string& bodyType)
		{
			if (bodyType == "Static")
			{
				return BodyType::Static;
			}
			else if (bodyType == "Dynamic")
			{
				return BodyType::Dynamic;
			}
			else if (bodyType == "Kinematic")
			{
				return BodyType::Kinematic;
			}

			return BodyType::Dynamic; // デフォルトはDynamicにする
		}

	}

	void RigidbodyComponent::Initialize()
	{
		rigidbody_ = std::make_unique<Rigidbody>();
		rigidbody_->SetBodyType(bodyType_);
		rigidbody_->SetMass(mass_);
		rigidbody_->SetUseGravity(useGravity_);
		rigidbody_->SetVelocity(velocity_);
		rigidbody_->SetAngularVelocity(angularVelocity_);
		rigidbody_->SetSleepEnabled(sleepEnabled_); // Editor上でDebug操作するためSleep機能は無効化する
		rigidbody_->SetRestitution(restitution_);
		rigidbody_->SetStaticFriction(staticFriction_);
		rigidbody_->SetDynamicFriction(dynamicFriction_);
	}

	void RigidbodyComponent::Update([[maybe_unused]] float deltaTime)
	{
		if (!rigidbody_)
		{
			return; // Rigidbody未生成の場合は更新しない
		}

		velocity_ = rigidbody_->GetVelocity(); // Debug表示用に現在速度を保持する
		angularVelocity_ = rigidbody_->GetAngularVelocity();
	}

	void RigidbodyComponent::PostPhysicsUpdate(float deltaTime)
	{
		if (!rigidbody_)
		{
			return; // Rigidbody未生成の場合は更新しない
		}

		velocity_ = rigidbody_->GetVelocity();
		angularVelocity_ = rigidbody_->GetAngularVelocity();

		if (rigidbody_->GetBodyType() != BodyType::Dynamic || deltaTime <= 0.0f)
		{
			return;
		}

		SceneComponent* root = GetTargetRootComponent();
		if (!root || root->GetParent())
		{
			return;
		}

		if (Vector3::LengthSquared(angularVelocity_) <= 0.00000001f)
		{
			return;
		}

		Vector3 rotation = root->GetLocalRotation();
		rotation += angularVelocity_ * deltaTime;
		root->SetLocalRotation(rotation);
		root->RefreshWorldTransform(); // Torqueで得た角速度をActorのRoot回転へ反映する。
	}

	void RigidbodyComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		// 物理状態をInspectorだけで確認できるよう、操作名と状態表示を日本語へ統一する。
		ImGui::SeparatorText("リジッドボディコンポーネント");

		ComponentPropertyUtility::DrawImGui(CreateProperties());

		if (rigidbody_ && ImGui::Button("リジッドボディを起こす"))
		{
			rigidbody_->WakeUp(); // Debug操作でSleepから復帰させる
		}

		if (rigidbody_)
		{
			ImGui::Text("スリープ中: %s", rigidbody_->IsSleeping() ? "はい" : "いいえ");
			ImGui::Text("接地中: %s", rigidbody_->IsGrounded() ? "はい" : "いいえ");
		}

#endif // USE_IMGUI
	}

	void RigidbodyComponent::Finalize()
	{
		// Rigidbodyの破棄はActorがComponentを所有コンテナから削除する際にunique_ptrへ任せる。
	}

	void RigidbodyComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // 基底クラスの共通情報を保存する

		outJson["Class"] = GetClassTypeName(); // RigidbodyComponentとして保存する

		ComponentPropertyUtility::ToJson(const_cast<RigidbodyComponent*>(this)->CreateProperties(), outJson);
	}

	void RigidbodyComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // 基底クラスの共通情報を復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void RigidbodyComponent::SetBodyType(BodyType bodyType)
	{
		bodyType_ = bodyType; // ImGui表示用のBodyTypeも更新する

		if (rigidbody_)
		{
			rigidbody_->SetBodyType(bodyType_);
		}
	}

	void RigidbodyComponent::SetMass(float mass)
	{
		mass_ = std::max(mass, 0.0001f); // ImGui表示用の質量も更新する

		if (rigidbody_)
		{
			rigidbody_->SetMass(mass_);
		}
	}

	void RigidbodyComponent::SetUseGravity(bool useGravity)
	{
		useGravity_ = useGravity; // ImGui表示用の重力フラグも更新する

		if (rigidbody_)
		{
			rigidbody_->SetUseGravity(useGravity_);
		}
	}

	void RigidbodyComponent::SetVelocity(const Vector3& velocity)
	{
		velocity_ = velocity; // ImGui表示用の速度も更新する

		if (rigidbody_)
		{
			rigidbody_->SetVelocity(velocity_);
			rigidbody_->WakeUp(); // 速度を設定した場合はSleep状態から復帰させる
		}
	}

	void RigidbodyComponent::SetAngularVelocity(const Vector3& angularVelocity)
	{
		angularVelocity_ = angularVelocity;

		if (rigidbody_)
		{
			rigidbody_->SetAngularVelocity(angularVelocity_);
			rigidbody_->WakeUp();
		}
	}

	void RigidbodyComponent::AddForce(const Vector3& force)
	{
		if (rigidbody_)
		{
			rigidbody_->AddForce(force); // 外部から加えた力をRigidbodyへ蓄積する
		}
	}

	void RigidbodyComponent::AddTorque(const Vector3& torque)
	{
		if (rigidbody_)
		{
			rigidbody_->AddTorque(torque);
		}
	}

	void RigidbodyComponent::WakeUp()
	{
		if (rigidbody_)
		{
			rigidbody_->WakeUp(); // RigidbodyをSleep状態から復帰させる
		}
	}

	void RigidbodyComponent::SetSleepEnabled(bool enabled)
	{
		sleepEnabled_ = enabled; // ImGui表示用のSleep機能有効状態も更新する

		if (rigidbody_)
		{
			rigidbody_->SetSleepEnabled(sleepEnabled_);
		}
	}

	void RigidbodyComponent::SetRestitution(float restitution)
	{
		restitution_ = std::clamp(restitution, 0.0f, 1.0f);
		if (rigidbody_)
		{
			rigidbody_->SetRestitution(restitution_);
		}
	}

	void RigidbodyComponent::SetStaticFriction(float staticFriction)
	{
		staticFriction_ = std::max(staticFriction, 0.0f);
		if (rigidbody_)
		{
			rigidbody_->SetStaticFriction(staticFriction_);
		}
	}

	void RigidbodyComponent::SetDynamicFriction(float dynamicFriction)
	{
		dynamicFriction_ = std::max(dynamicFriction, 0.0f);
		if (rigidbody_)
		{
			rigidbody_->SetDynamicFriction(dynamicFriction_);
		}
	}

	SceneComponent* RigidbodyComponent::GetTargetRootComponent() const
	{
		const Actor* owner = GetOwner();
		if (!owner)
		{
			return nullptr; // 所有者Actorが無い場合はRootComponentを取得できない
		}

		return owner->GetRootComponent(); // 所有者ActorのRootComponentを返す
	}

	std::vector<ComponentProperty> RigidbodyComponent::CreateProperties()
	{
		return {
			{ "BodyType", "ボディ種類", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return std::string(ToString(bodyType_)); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetBodyType(BodyTypeFromString(*typedValue)); } }, 0.0f, 0.0f, 0.1f, false, { { "Static", "静的" }, { "Dynamic", "動的" }, { "Kinematic", "キネマティック" } } },
			{ "Mass", "質量", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return mass_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMass(*typedValue); } }, 0.0001f, 1000.0f, 0.05f, true },
			{ "UseGravity", "重力を使用", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return useGravity_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetUseGravity(*typedValue); } } },
			{ "Velocity", "速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector3>(&value)) { SetVelocity(*typedValue); } }, 0.0f, 0.0f, 0.05f },
			{ "AngularVelocity", "角速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return angularVelocity_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector3>(&value)) { SetAngularVelocity(*typedValue); } }, 0.0f, 0.0f, 0.05f },
			{ "SleepEnabled", "スリープを有効化", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return sleepEnabled_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetSleepEnabled(*typedValue); } } },
			{ "Restitution", "反発係数", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return restitution_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetRestitution(*typedValue); } }, 0.0f, 1.0f, 0.01f, true },
			{ "StaticFriction", "静止摩擦", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return staticFriction_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetStaticFriction(*typedValue); } }, 0.0f, 10.0f, 0.01f, true },
			{ "DynamicFriction", "動摩擦", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return dynamicFriction_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetDynamicFriction(*typedValue); } }, 0.0f, 10.0f, 0.01f, true }
		};
	}

}
