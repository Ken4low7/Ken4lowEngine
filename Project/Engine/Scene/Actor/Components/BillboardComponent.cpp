#define NOMINMAX
#include "BillboardComponent.h"

#include "AssetPathSelector.h"
#include "CameraManager.h"
#include "Matrix4x4.h"
#include "Object3D.h"
#include "Vector3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kBillboardQuadModelPath = "Sample/plane.gltf";

		Vector2 ReadVector2FromJson(const nlohmann::json& json, const char* key, const Vector2& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 2)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>()
			};
		}

		Vector4 ReadVector4FromJson(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>(),
				json[key][3].get<float>()
			};
		}
	}

	BillboardComponent::~BillboardComponent() = default;

	void BillboardComponent::Initialize()
	{
		SceneComponent::Initialize();
		EnsureObject3D();
		ApplyBillboardTransform();
	}

	void BillboardComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		EnsureObject3D();
		ApplyBillboardTransform();
	}

	void BillboardComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		EnsureObject3D();
		ApplyBillboardTransform();
	}

	void BillboardComponent::Draw()
	{
		if (!visible_ || !IsActiveInHierarchy())
		{
			return; // 非表示または無効なBillboardComponentは描画しない
		}

		if (!object3D_)
		{
			return; // 描画対象が生成できない場合は描画しない
		}

		ApplyBillboardTransform();
		object3D_->Draw(); // 3D描画パスでBillboardを描画する
	}

	void BillboardComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();

#ifdef USE_IMGUI
		ImGui::SeparatorText("ビルボードコンポーネント");

		std::array<char, 256> texturePathBuffer{};
		std::snprintf(texturePathBuffer.data(), texturePathBuffer.size(), "%s", texturePath_.c_str());
		if (ImGui::InputText("テクスチャパス", texturePathBuffer.data(), texturePathBuffer.size()))
		{
			SetTexturePath(texturePathBuffer.data());
		}

		std::string selectedTexturePath = texturePath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##BillboardComponentTexturePath", selectedTexturePath, AssetType::Texture))
		{
			SetTexturePath(selectedTexturePath);
		}

		ImGui::DragFloat2("サイズ", &size_.x, 0.01f, 0.0f, 1000.0f);
		ImGui::ColorEdit4("色", &color_.x);
		ImGui::Checkbox("表示", &visible_);
		ImGui::Checkbox("Y軸だけ回転", &lockYAxis_);
		ImGui::DragFloat("回転オフセット", &rotationOffset_, 0.01f);
#endif // USE_IMGUI
	}

	void BillboardComponent::Finalize()
	{
		// Object3Dの破棄はActorがComponentを所有コンテナから削除する際にunique_ptrへ任せる。
		loadedTexturePath_.clear();
	}

	void BillboardComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // BillboardComponentとして保存する
		outJson["TexturePath"] = texturePath_;
		outJson["Size"] = { size_.x, size_.y };
		outJson["Color"] = { color_.x, color_.y, color_.z, color_.w };
		outJson["Visible"] = visible_;
		outJson["LockYAxis"] = lockYAxis_;
		outJson["RotationOffset"] = rotationOffset_;
	}

	void BillboardComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("TexturePath") && inJson["TexturePath"].is_string())
		{
			SetTexturePath(inJson["TexturePath"].get<std::string>());
		}

		size_ = ReadVector2FromJson(inJson, "Size", size_);
		color_ = ReadVector4FromJson(inJson, "Color", color_);

		if (inJson.contains("Visible") && inJson["Visible"].is_boolean())
		{
			visible_ = inJson["Visible"].get<bool>();
		}

		if (inJson.contains("LockYAxis") && inJson["LockYAxis"].is_boolean())
		{
			lockYAxis_ = inJson["LockYAxis"].get<bool>();
		}

		if (inJson.contains("RotationOffset") && inJson["RotationOffset"].is_number())
		{
			rotationOffset_ = inJson["RotationOffset"].get<float>();
		}
	}

	void BillboardComponent::SetTexturePath(const std::string& texturePath)
	{
		if (texturePath_ == texturePath)
		{
			return; // 同じTextureなら再設定しない
		}

		texturePath_ = texturePath;
		loadedTexturePath_.clear();
	}

	void BillboardComponent::EnsureObject3D()
	{
		if (!object3D_)
		{
			object3D_ = std::make_unique<Object3D>();
			object3D_->Initialize(kBillboardQuadModelPath);
			object3D_->SetFrustumCullingEnabled(false);
		}

		if (object3D_ && !texturePath_.empty() && loadedTexturePath_ != texturePath_)
		{
			object3D_->SetTextureForAll(texturePath_); // Billboardに使用するTextureを反映する
			loadedTexturePath_ = texturePath_;
		}
	}

	void BillboardComponent::ApplyBillboardTransform()
	{
		if (!object3D_)
		{
			return; // 描画対象がない場合はTransformを反映しない
		}

		object3D_->SetColor(color_);
		object3D_->UpdateWithWorldMatrix(BuildBillboardWorldMatrix());
	}

	Matrix4x4 BillboardComponent::BuildBillboardWorldMatrix() const
	{
		const Vector3 worldScale = GetWorldScale();
		const Vector3 scale{
			std::max(size_.x, 0.0f) * worldScale.x,
			std::max(size_.y, 0.0f) * worldScale.y,
			std::max(worldScale.z, 0.0001f)
		};

		Matrix4x4 facingMatrix = Matrix4x4::MakeIdentity();

		if (lockYAxis_)
		{
			const Vector3 toCamera = CameraManager::GetInstance()->GetActiveCameraPosition() - GetWorldPosition();
			const Vector3 direction = Vector3::NormalizeXZSafe(toCamera);
			const float yaw = std::atan2(-direction.x, direction.z);
			facingMatrix = Matrix4x4::MakeRotateY(yaw);
		}
		else
		{
			Matrix4x4 cameraWorld = Matrix4x4::Inverse(CameraManager::GetInstance()->GetActiveViewMatrix());
			cameraWorld.m[3][0] = 0.0f;
			cameraWorld.m[3][1] = 0.0f;
			cameraWorld.m[3][2] = 0.0f;
			facingMatrix = cameraWorld;
		}

		const Matrix4x4 scaleMatrix = Matrix4x4::MakeScaleMatrix(scale);
		const Matrix4x4 rotationOffsetMatrix = Matrix4x4::MakeRotateZMatrix(rotationOffset_);
		const Matrix4x4 translationMatrix = Matrix4x4::MakeTranslateMatrix(GetWorldPosition());

		return Matrix4x4::Multiply(
			Matrix4x4::Multiply(
				Matrix4x4::Multiply(scaleMatrix, rotationOffsetMatrix),
				facingMatrix),
			translationMatrix);
	}
}
