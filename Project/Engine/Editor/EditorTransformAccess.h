#pragma once

#include "EditorObjectInfo.h"

#include "Camera.h"
#include "LightManager.h"
#include "Sprite.h"

#include <cstddef>
#include <string>
#include <utility>

namespace Ken4lowEngine
{

	// Editor DetailsからCamera Transformへ触る入口を1箇所に集約し、Scene側のraw pointer保持を最小化する。
	inline EditorObjectInfo MakeCameraEditorObject(uint64_t id, std::string displayName, std::string typeName, std::string sceneName, Camera* camera)
	{
		EditorObjectInfo object{ id, std::move(displayName), std::move(typeName), std::move(sceneName) };
		object.inspectorType = EditorInspectorType::Transform;
		if (!camera)
		{
			object.transformUnavailableReason = "Camera is not created yet.";
			return object;
		}

		object.canEditTransform = true;
		object.readTransform = [camera](EditorTransform& transform)
		{
			if (!camera)
			{
				return false;
			}
			transform.position = camera->GetTranslate();
			transform.rotation = camera->GetRotate();
			transform.scale = camera->GetScale();
			return true;
		};
		object.writeTransform = [camera](const EditorTransform& transform)
		{
			if (!camera)
			{
				return;
			}
			camera->SetTranslate(transform.position);
			camera->SetRotate(transform.rotation);
			camera->SetScale(transform.scale);
			camera->Update();
		};
		return object;
	}

	// Sprite/UIは2D値をDetailsのPosition/Rotation/Scaleへ写像し、編集後はSprite::Updateまで行う。
	inline EditorObjectInfo MakeSpriteEditorObject(uint64_t id, std::string displayName, std::string typeName, std::string sceneName, Sprite* sprite)
	{
		EditorObjectInfo object{ id, std::move(displayName), std::move(typeName), std::move(sceneName) };
		object.inspectorType = EditorInspectorType::Transform;
		if (!sprite)
		{
			object.transformUnavailableReason = "Sprite is not created yet.";
			return object;
		}

		object.canEditTransform = true;
		object.readTransform = [sprite](EditorTransform& transform)
		{
			if (!sprite)
			{
				return false;
			}
			const Vector2& position = sprite->GetPosition();
			const Vector2& size = sprite->GetSize();
			transform.position = { position.x, position.y, 0.0f };
			transform.rotation = { 0.0f, 0.0f, sprite->GetRotation() };
			transform.scale = { size.x, size.y, 1.0f };
			return true;
		};
		object.writeTransform = [sprite](const EditorTransform& transform)
		{
			if (!sprite)
			{
				return;
			}
			sprite->SetPosition({ transform.position.x, transform.position.y });
			sprite->SetRotation(transform.rotation.z);
			sprite->SetSize({ transform.scale.x, transform.scale.y });
			sprite->Update();
		};
		return object;
	}

	// LightManager内のライトは毎回index検証して、Scene切り替えやライト再生成後の古い入口を安全に無効化する。
	inline EditorObjectInfo MakePunctualLightEditorObject(uint64_t id, std::string displayName, std::string typeName, std::string sceneName, size_t lightIndex)
	{
		EditorObjectInfo object{ id, std::move(displayName), std::move(typeName), std::move(sceneName) };
		object.canEditTransform = lightIndex < LightManager::GetInstance()->GetPunctualLights().size();
		object.inspectorType = EditorInspectorType::Transform;
		object.transformUnavailableReason = "Light is not created yet.";
		object.readTransform = [lightIndex](EditorTransform& transform)
		{
			LightManager* lightManager = LightManager::GetInstance();
			const auto& lights = lightManager->GetPunctualLights();
			if (lightIndex >= lights.size())
			{
				return false;
			}

			const auto& light = lights[lightIndex];
			transform.position = light.position;
			transform.rotation = light.direction;
			transform.scale = { light.radius, light.distance, light.intensity };
			return true;
		};
		object.writeTransform = [lightIndex](const EditorTransform& transform)
		{
			LightManager* lightManager = LightManager::GetInstance();
			auto& lights = lightManager->GetMutablePunctualLightsForEditor();
			if (lightIndex >= lights.size())
			{
				return;
			}

			auto& light = lights[lightIndex];
			light.position = transform.position;
			light.direction = transform.rotation;
			light.radius = transform.scale.x;
			light.distance = transform.scale.y;
			light.intensity = transform.scale.z;
		};
		return object;
	}

} // namespace Ken4lowEngine
