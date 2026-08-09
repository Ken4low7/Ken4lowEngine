#pragma once

#include "ColliderComponent.h"

#include <algorithm>

namespace Ken4lowEngine
{
	/// Blender Level由来のcollision_type / collision_type_idを保持するStage専用ColliderComponent。
	class StageColliderComponent final : public ColliderComponent
	{
	public:
		std::string GetClassTypeName() const override
		{
			return "StageColliderComponent";
		}

		void Initialize() override
		{
			ColliderComponent::Initialize();
			// Base初期化でCollider実体が生成された後にLevel由来の識別情報を同期する。
			SetCollisionTypeId(GetCollisionTypeId());
			SetCollisionTag(GetCollisionTag());
		}

		void ToJson(nlohmann::json& outJson) const override
		{
			ColliderComponent::ToJson(outJson);
			outJson["Class"] = GetClassTypeName();
			outJson["CollisionTypeId"] = GetCollisionTypeId();
			outJson["CollisionTag"] = GetCollisionTag();
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ColliderComponent::FromJson(inJson);
			if (inJson.contains("CollisionTypeId") && inJson["CollisionTypeId"].is_number_integer())
			{
				SetCollisionTypeId(static_cast<uint32_t>((std::max)(inJson["CollisionTypeId"].get<int>(), 0)));
			}
			if (inJson.contains("CollisionTag") && inJson["CollisionTag"].is_string())
			{
				SetCollisionTag(inJson["CollisionTag"].get<std::string>());
			}
		}
	};
} // namespace Ken4lowEngine
