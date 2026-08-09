#pragma once

#include <Actor.h>
#include <Object3D.h>
#include <SceneComponent.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace K4E = ::Ken4lowEngine;

/// Stage 2の装置本体を描画し、未起動・注目中・起動済みの見た目を切り替えるComponent。
class Stage2DeviceVisualComponent final : public K4E::SceneComponent
{
public:
	std::string GetClassTypeName() const override { return "Stage2DeviceVisualComponent"; }

	void Initialize() override
	{
		K4E::SceneComponent::Initialize();
		baseObject_ = CreateCube();
		coreObject_ = CreateCube();
		beaconObject_ = CreateCube();
		SyncVisuals(); // 初回描画前に装置のTransformとMaterialを確定する。
	}

	void Update(float deltaTime) override
	{
		K4E::SceneComponent::Update(deltaTime);
		visualTimer_ += std::max(0.0f, deltaTime);
		if (activationPulseTimer_ > 0.0f)
		{
			activationPulseTimer_ = std::max(0.0f, activationPulseTimer_ - deltaTime);
		}
		SyncVisuals();
	}

	void UpdateEditor(float deltaTime) override
	{
		K4E::SceneComponent::UpdateEditor(deltaTime);
		visualTimer_ += std::max(0.0f, deltaTime);
		SyncVisuals();
	}

	void Draw() override
	{
		if (baseObject_) baseObject_->Draw();
		if (coreObject_) coreObject_->Draw();
		if (beaconObject_) beaconObject_->Draw();
	}

	void DrawShadow() override
	{
		if (baseObject_) baseObject_->DrawShadow();
		if (coreObject_) coreObject_->DrawShadow();
	}

	void Finalize() override
	{
		beaconObject_.reset();
		coreObject_.reset();
		baseObject_.reset();
		K4E::SceneComponent::Finalize();
	}

	void SetFocused(bool focused) { focused_ = focused; }
	void SetInteractionProgress(float progress) { interactionProgress_ = std::clamp(progress, 0.0f, 1.0f); }
	void SetActivated(bool activated)
	{
		if (activated && !activated_)
		{
			activationPulseTimer_ = 0.8f;
		}
		activated_ = activated;
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (baseObject_) baseObject_->UpdateShadowMatrix(lightViewProjection);
		if (coreObject_) coreObject_->UpdateShadowMatrix(lightViewProjection);
	}

private:
	static std::unique_ptr<K4E::Object3D> CreateCube()
	{
		try
		{
			auto object = std::make_unique<K4E::Object3D>();
			object->Initialize("Sample/cube.gltf");
			object->SetPbrEnabled(true);
			object->SetMetallic(0.35f);
			object->SetRoughness(0.24f);
			object->SetReflectivity(0.58f);
			object->SetFrustumCullingEnabled(false);
			return object;
		}
		catch (...)
		{
			return nullptr; // 装置モデルの生成失敗時もObjective進行自体は継続できるようにする。
		}
	}

	void SyncVisuals()
	{
		const K4E::Vector3 position = GetWorldPosition();
		const float focusPulse = focused_ ? 0.5f + 0.5f * std::sin(visualTimer_ * 5.5f) : 0.0f;
		const float activePulse = activated_ ? 0.5f + 0.5f * std::sin(visualTimer_ * 3.2f) : 0.0f;
		const float completionPulse = activationPulseTimer_ > 0.0f
			? std::sin((1.0f - activationPulseTimer_ / 0.8f) * 3.14159265f)
			: 0.0f;

		if (baseObject_)
		{
			baseObject_->SetTranslate(position + K4E::Vector3{ 0.0f, 1.0f, 0.0f });
			baseObject_->SetRotate({ 0.0f, visualTimer_ * (activated_ ? 0.18f : 0.04f), 0.0f });
			baseObject_->SetScale({ 1.15f, 1.0f, 1.15f });
			baseObject_->SetColor(activated_
				? K4E::Vector4{ 0.18f, 0.62f, 0.28f, 1.0f }
				: K4E::Vector4{ 0.16f, 0.25f + focusPulse * 0.08f, 0.34f, 1.0f });
			baseObject_->SetEmissiveFactor(activated_
				? K4E::Vector4{ 0.08f, 0.75f + activePulse * 0.35f, 0.20f, 1.0f }
				: K4E::Vector4{ 0.04f, 0.18f + focusPulse * 0.25f, 0.30f + focusPulse * 0.35f, 1.0f });
			baseObject_->Update();
		}

		if (coreObject_)
		{
			const float chargeScale = 0.52f + interactionProgress_ * 0.18f + focusPulse * 0.04f + completionPulse * 0.30f;
			coreObject_->SetTranslate(position + K4E::Vector3{ 0.0f, 2.35f, 0.0f });
			coreObject_->SetRotate({ visualTimer_ * 0.42f, visualTimer_ * (activated_ ? 1.45f : 0.72f), visualTimer_ * 0.25f });
			coreObject_->SetScale({ chargeScale, chargeScale, chargeScale });
			coreObject_->SetColor(activated_
				? K4E::Vector4{ 0.28f, 1.0f, 0.45f, 1.0f }
				: K4E::Vector4{ 0.22f, 0.72f + interactionProgress_ * 0.25f, 1.0f, 1.0f });
			coreObject_->SetEmissiveFactor(activated_
				? K4E::Vector4{ 0.45f, 3.0f + activePulse * 1.2f, 0.75f, 1.0f }
				: K4E::Vector4{ 0.18f, 1.2f + interactionProgress_ * 1.8f, 2.8f + focusPulse, 1.0f });
			coreObject_->Update();
		}

		if (beaconObject_)
		{
			const float beaconScale = 0.72f + interactionProgress_ * 0.42f + activePulse * 0.08f + completionPulse * 0.35f;
			beaconObject_->SetTranslate(position + K4E::Vector3{ 0.0f, 3.45f, 0.0f });
			beaconObject_->SetRotate({ 0.0f, visualTimer_ * (activated_ ? 1.8f : 0.9f), 0.0f });
			beaconObject_->SetScale({ beaconScale, 0.07f, beaconScale });
			beaconObject_->SetColor(activated_
				? K4E::Vector4{ 0.35f, 1.0f, 0.56f, 0.92f }
				: K4E::Vector4{ 0.30f, 0.82f, 1.0f, focused_ ? 0.94f : 0.62f });
			beaconObject_->SetEmissiveFactor(activated_
				? K4E::Vector4{ 0.4f, 3.4f, 0.7f, 1.0f }
				: K4E::Vector4{ 0.2f, 1.4f + focusPulse, 3.0f + focusPulse, 1.0f });
			beaconObject_->Update();
		}
	}

	std::unique_ptr<K4E::Object3D> baseObject_;
	std::unique_ptr<K4E::Object3D> coreObject_;
	std::unique_ptr<K4E::Object3D> beaconObject_;
	float visualTimer_ = 0.0f;
	float interactionProgress_ = 0.0f;
	float activationPulseTimer_ = 0.0f;
	bool focused_ = false;
	bool activated_ = false;
};

/// 装置ID、起動長押し進捗、起動済み状態をまとめるStage 2専用Actor。
class Stage2DeviceActor final : public K4E::Actor
{
public:
	std::string GetClassTypeName() const override { return "Stage2DeviceActor"; }

	void Initialize() override
	{
		if (!GetComponents().empty())
		{
			K4E::Actor::Initialize();
			visualComponent_ = GetComponent<Stage2DeviceVisualComponent>();
			return;
		}

		auto& visual = CreateRootComponent<Stage2DeviceVisualComponent>();
		visual.SetName("Stage 2 Device Visual");
		visual.SetUpdateOrder(0);
		visual.SetDrawOrder(0);
		visualComponent_ = &visual;
		K4E::Actor::Initialize();
	}

	void Configure(std::string deviceId, const K4E::Vector3& position)
	{
		deviceId_ = std::move(deviceId);
		if (deviceId_.empty()) deviceId_ = "Stage2Device";
		SetName(deviceId_);
		SetLayer("StageObjective");
		AddTag("Stage2Device");
		if (auto* root = GetRootComponent())
		{
			root->SetLocalPosition(position);
			root->RefreshWorldTransform();
		}
	}

	bool UpdateInteraction(bool focused, bool interactHeld, float deltaTime)
	{
		focused_ = focused && !activated_;
		if (visualComponent_) visualComponent_->SetFocused(focused_);
		if (activated_)
		{
			interactionProgress_ = 1.0f;
			if (visualComponent_) visualComponent_->SetInteractionProgress(interactionProgress_);
			return false;
		}

		const float safeDeltaTime = std::max(0.0f, deltaTime);
		if (focused_ && interactHeld)
		{
			interactionProgress_ += safeDeltaTime / std::max(0.05f, interactionHoldDuration_);
		}
		else
		{
			interactionProgress_ -= safeDeltaTime * 1.8f / std::max(0.05f, interactionHoldDuration_);
		}
		interactionProgress_ = std::clamp(interactionProgress_, 0.0f, 1.0f);
		if (visualComponent_) visualComponent_->SetInteractionProgress(interactionProgress_);

		if (interactionProgress_ < 1.0f) return false;

		activated_ = true;
		focused_ = false;
		if (visualComponent_)
		{
			visualComponent_->SetFocused(false);
			visualComponent_->SetActivated(true); // 起動成立フレームで緑発光と完了パルスへ切り替える。
		}
		return true;
	}

	float GetDistanceSquaredTo(const K4E::Vector3& position) const
	{
		const K4E::Vector3 delta = GetPosition() - position;
		return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4& lightViewProjection)
	{
		if (visualComponent_) visualComponent_->UpdateShadowMatrix(lightViewProjection);
	}

	const std::string& GetDeviceId() const { return deviceId_; }
	const K4E::Vector3& GetPosition() const
	{
		static const K4E::Vector3 zero{};
		return GetRootComponent() ? GetRootComponent()->GetWorldPosition() : zero;
	}
	float GetInteractionProgress() const { return interactionProgress_; }
	float GetInteractionRadius() const { return interactionRadius_; }
	bool IsActivated() const { return activated_; }

private:
	Stage2DeviceVisualComponent* visualComponent_ = nullptr;
	std::string deviceId_;
	float interactionProgress_ = 0.0f;
	float interactionHoldDuration_ = 1.25f;
	float interactionRadius_ = 3.8f;
	bool focused_ = false;
	bool activated_ = false;
};
