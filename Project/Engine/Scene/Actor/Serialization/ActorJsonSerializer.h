#pragma once

#include "ActorSpawnOptions.h"

#include <json.hpp>
#include <memory>
#include <string_view>

namespace Ken4lowEngine
{
	class Actor;

	/// <summary>Actor・ActorComponentのJSONシリアライズを行うクラスです。</summary>
	class ActorJsonSerializer
	{
	public:
		static bool SaveActorToFile(const Actor& actor, std::string_view filePath);
		static bool LoadActorFromFile(Actor& actor, std::string_view filePath);
		static std::unique_ptr<Actor> CreateActorFromJson(std::string_view filePath, const ActorSpawnOptions& options = {});

		static nlohmann::json SerializeActor(const Actor& actor);
		static bool ValidateActorDefinition(const nlohmann::json& actorJson); // World差し替え前にActor/ComponentのFactory登録不足を副作用なしで検証する。
		static bool LoadActorFromJson(Actor& actor, const nlohmann::json& actorJson);
		static std::unique_ptr<Actor> CreateActorFromJson(const nlohmann::json& actorJson, const ActorSpawnOptions& options = {});
	};
} // namespace Ken4lowEngine
