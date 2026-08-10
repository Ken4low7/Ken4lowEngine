#include "LevelSerializer.h"

#include "ActorJsonSerializer.h"
#include "ActorWorld.h"
#include "LevelVersionMigration.h"
#include "PrefabInstanceRegistry.h"
#include "PrefabReferenceResolver.h"
#include "SceneComponent.h"

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#endif

#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		Actor* GetParentActor(const Actor& actor)
		{
			SceneComponent* root = actor.GetRootComponent();
			SceneComponent* parent = root ? root->GetParent() : nullptr;
			Actor* parentActor = parent ? parent->GetOwner() : nullptr;
			return parentActor != &actor ? parentActor : nullptr;
		}
	}

	LevelSerializer::Result LevelSerializer::LoadFromFile(const std::filesystem::path& path, LevelDocument& outDocument)
	{
		Result result{};
		try
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				result.message = "Levelファイルを開けません: " + path.generic_string();
				return result;
			}

			nlohmann::json sourceJson;
			file >> sourceJson;
			std::string error;
			if (!Deserialize(sourceJson, path.parent_path(), outDocument, error))
			{
				result.message = error + ": " + path.generic_string();
				return result;
			}

			result.succeeded = true;
			result.sourceVersion = outDocument.sourceVersion;
			result.migrated = outDocument.migrated;
			result.message = outDocument.migrated
				? "LevelをVersion " + std::to_string(outDocument.sourceVersion) + " から " +
					std::to_string(LevelDocument::kCurrentVersion) + " へ移行して読み込みました: " + path.generic_string()
				: "LevelDocumentを読み込みました: " + path.generic_string();
			return result;
		}
		catch (const std::exception& exception)
		{
			result.message = "Level読込中に例外が発生しました: " + std::string(exception.what());
			return result;
		}
	}

	LevelSerializer::Result LevelSerializer::SaveToFileAtomic(const std::filesystem::path& path, const LevelDocument& document)
	{
		Result result{};
		if (path.empty())
		{
			result.message = "Level保存パスが空です。";
			return result;
		}

		const std::filesystem::path temporaryPath = path.string() + ".tmp";
		try
		{
			if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
			{
				std::ofstream file(temporaryPath, std::ios::trunc);
				if (!file.is_open())
				{
					result.message = "Level一時ファイルを作成できません: " + temporaryPath.generic_string();
					return result;
				}
				file << Serialize(document).dump(4);
				if (!file.good())
				{
					result.message = "Level一時ファイルの書き込みに失敗しました: " + temporaryPath.generic_string();
					return result;
				}
			}

			if (std::filesystem::exists(path)) std::filesystem::remove(path);
			std::filesystem::rename(temporaryPath, path); // 完全に書き切ったファイルだけを本体へ昇格する。
			result.succeeded = true;
			result.sourceVersion = LevelDocument::kCurrentVersion;
			result.message = "LevelDocumentを保存しました: " + path.generic_string();
			return result;
		}
		catch (const std::exception& exception)
		{
			std::error_code cleanupError;
			std::filesystem::remove(temporaryPath, cleanupError);
			result.message = "Level保存中に例外が発生しました: " + std::string(exception.what());
			return result;
		}
	}

	bool LevelSerializer::Deserialize(
		const nlohmann::json& sourceJson,
		const std::filesystem::path& levelBaseDirectory,
		LevelDocument& outDocument,
		std::string& outError)
	{
		outError.clear();
		nlohmann::json levelJson = sourceJson;
		uint32_t sourceVersion = 0;
		if (!LevelVersionMigration::MigrateToCurrent(levelJson, sourceVersion, outError)) return false;
		if (!levelJson.contains("Actors") || !levelJson["Actors"].is_array())
		{
			outError = "LevelのActorsが配列ではありません。";
			return false;
		}

		LevelDocument document{};
		document.sourceVersion = sourceVersion;
		document.migrated = sourceVersion != LevelDocument::kCurrentVersion;
		document.name = levelJson.value("Name", "Untitled");
		if (levelJson.contains("LevelSettings") && levelJson["LevelSettings"].is_object())
		{
			document.targetScene = levelJson["LevelSettings"].value("TargetScene", std::string{});
		}
		document.lighting = levelJson.value("Lighting", nlohmann::json::object());
		document.camera = levelJson.value("Camera", nlohmann::json::object());
		document.environment = levelJson.value("Environment", nlohmann::json::object());

		std::size_t actorIndex = 0;
		for (const nlohmann::json& entry : levelJson["Actors"])
		{
			if (!entry.is_object())
			{
				outError = "Level内Actorエントリがobjectではありません。";
				return false;
			}

			LevelActorDocument actor{};
			actor.id = entry.value("Id", "Actor_" + std::to_string(actorIndex));
			actor.parentId = entry.value("ParentId", std::string{});
			if (entry.contains("Editor") && entry["Editor"].is_object())
			{
				const nlohmann::json& editor = entry["Editor"];
				actor.editorVisible = editor.value("Visible", true);
				actor.editorLocked = editor.value("Locked", false);
				actor.editorFolder = editor.value("Folder", std::string{});
			}

			if (entry.contains("Prefab"))
			{
				if (!entry["Prefab"].is_object())
				{
					outError = "ActorのPrefab指定がobjectではありません: " + actor.id;
					return false;
				}
				const nlohmann::json& prefabJson = entry["Prefab"];
				actor.prefab.path = prefabJson.value("Path", std::string{});
				actor.prefab.overrides = prefabJson.value("Overrides", nlohmann::json::object());
				if (actor.prefab.path.empty())
				{
					outError = "ActorのPrefab Pathが空です: " + actor.id;
					return false;
				}

				const PrefabReferenceResolver::Result prefabResult = PrefabReferenceResolver::Resolve(
					actor.prefab.path, actor.prefab.overrides, levelBaseDirectory);
				if (!prefabResult.succeeded)
				{
					outError = actor.id + ": " + prefabResult.message;
					return false;
				}
				actor.resolvedData = prefabResult.actorJson;
			}
			else
			{
				if (!entry.contains("Data") || !entry["Data"].is_object())
				{
					outError = "ActorにDataまたはPrefabがありません: " + actor.id;
					return false;
				}
				actor.data = entry["Data"];
				actor.resolvedData = actor.data;
			}

			if (!ActorJsonSerializer::ValidateActorDefinition(actor.resolvedData))
			{
				outError = "Actor定義の検証に失敗しました: " + actor.id;
				return false;
			}
			document.actors.push_back(std::move(actor));
			++actorIndex;
		}

		if (!ValidateActorGraph(document, outError)) return false;
		outDocument = std::move(document);
		return true;
	}

	nlohmann::json LevelSerializer::Serialize(const LevelDocument& document)
	{
		nlohmann::json level;
		level["Format"] = "Ken4lowLevel";
		level["Version"] = LevelDocument::kCurrentVersion;
		level["Name"] = document.name;
		level["LevelSettings"] = {
			{ "TargetScene", document.targetScene },
		};
		level["Actors"] = nlohmann::json::array();

		for (const LevelActorDocument& actor : document.actors)
		{
			nlohmann::json entry = {
				{ "Id", actor.id },
				{ "ParentId", actor.parentId },
				{ "Editor", {
					{ "Visible", actor.editorVisible },
					{ "Locked", actor.editorLocked },
					{ "Folder", actor.editorFolder },
				} },
			};

			if (actor.prefab.IsSet())
			{
				entry["Prefab"] = {
					{ "Path", actor.prefab.path },
					{ "Overrides", actor.prefab.overrides.is_null() ? nlohmann::json::object() : actor.prefab.overrides },
				};
			}
			else
			{
				entry["Data"] = actor.data;
			}
			level["Actors"].push_back(std::move(entry));
		}

		level["Lighting"] = document.lighting.is_null() ? nlohmann::json::object() : document.lighting;
		level["Camera"] = document.camera.is_null() ? nlohmann::json::object() : document.camera;
		level["Environment"] = document.environment.is_null() ? nlohmann::json::object() : document.environment;
		return level;
	}

	LevelDocument LevelSerializer::CaptureWorld(
		const ActorWorld& actorWorld,
		std::string_view levelName,
		std::string_view targetScene,
		nlohmann::json lighting,
		nlohmann::json camera,
		nlohmann::json environment)
	{
		LevelDocument document{};
		document.name = levelName.empty() ? "Untitled" : std::string(levelName);
		document.targetScene = std::string(targetScene);
		document.lighting = std::move(lighting);
		document.camera = std::move(camera);
		document.environment = std::move(environment);

		std::unordered_map<const Actor*, std::string> actorIds;
		std::size_t actorIndex = 0;
		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy()) continue;
			actorIds.emplace(actor, "Actor_" + std::to_string(actorIndex++));
		}

		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy()) continue;

			LevelActorDocument actorDocument{};
			actorDocument.id = actorIds.at(actor);
			if (Actor* parentActor = GetParentActor(*actor))
			{
				const auto parentIt = actorIds.find(parentActor);
				if (parentIt != actorIds.end()) actorDocument.parentId = parentIt->second;
			}

#ifdef USE_IMGUI
			const EditorActorState editorState = EditorActorStateRegistry::GetInstance()->GetState(actor);
			actorDocument.editorVisible = editorState.visible;
			actorDocument.editorLocked = editorState.locked;
			actorDocument.editorFolder = editorState.folderPath;
#endif

			const nlohmann::json actorJson = ActorJsonSerializer::SerializeActor(*actor);
			actorDocument.data = actorJson;
			actorDocument.resolvedData = actorJson;

			std::string prefabPath;
			if (PrefabInstanceRegistry::GetInstance()->Find(actor, prefabPath))
			{
				nlohmann::json baseActorJson;
				std::string prefabError;
				if (PrefabReferenceResolver::LoadBaseActor(prefabPath, {}, baseActorJson, prefabError))
				{
					actorDocument.prefab.path = prefabPath;
					actorDocument.prefab.overrides = PrefabReferenceResolver::BuildOverrides(baseActorJson, actorJson);
				}
				// Prefabが消えていた場合はData直書きへ自動Fallbackし、Level保存自体を失敗させない。
			}

			document.actors.push_back(std::move(actorDocument));
		}
		return document;
	}

	bool LevelSerializer::ValidateActorGraph(const LevelDocument& document, std::string& outError)
	{
		std::unordered_map<std::string, std::string> parentByActorId;
		for (const LevelActorDocument& actor : document.actors)
		{
			if (actor.id.empty())
			{
				outError = "Level内ActorのIdが空です。";
				return false;
			}
			if (!parentByActorId.emplace(actor.id, actor.parentId).second)
			{
				outError = "Level内ActorのIdが重複しています: " + actor.id;
				return false;
			}
		}

		for (const auto& [actorId, parentId] : parentByActorId)
		{
			if (parentId.empty()) continue;
			if (actorId == parentId)
			{
				outError = "Actorが自分自身をParentIdに指定しています: " + actorId;
				return false;
			}
			if (!parentByActorId.contains(parentId))
			{
				outError = "存在しないParentIdが指定されています: " + parentId;
				return false;
			}
		}

		for (const auto& [actorId, unusedParentId] : parentByActorId)
		{
			(void)unusedParentId;
			std::unordered_set<std::string> ancestry;
			std::string currentId = actorId;
			while (!currentId.empty())
			{
				if (!ancestry.insert(currentId).second)
				{
					outError = "Actor間の親子関係が循環しています: " + actorId;
					return false;
				}
				const auto parentIt = parentByActorId.find(currentId);
				currentId = parentIt != parentByActorId.end() ? parentIt->second : std::string{};
			}
		}
		return true;
	}
} // namespace Ken4lowEngine
