#include <EditorPrefabDiff.h>

#include <algorithm>
#include <cassert>
#include <iostream>

using Ken4lowEngine::EditorPrefabDiff;
using Ken4lowEngine::EditorPrefabDiffEntry;
using Ken4lowEngine::EditorPrefabDiffKind;

namespace
{
	bool HasEntry(
		const std::vector<EditorPrefabDiffEntry>& entries,
		EditorPrefabDiffKind kind,
		const char* componentName,
		const char* propertyPath)
	{
		return std::any_of(entries.begin(), entries.end(), [&](const EditorPrefabDiffEntry& entry)
			{
				return entry.kind == kind &&
					entry.componentName == componentName &&
					entry.propertyPath == propertyPath;
			});
	}

	void TestSemanticActorAndComponentDiff()
	{
		const nlohmann::json base = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Name": "BaseEnemy",
			"Tags": { "Team": "Enemy", "Boss": false },
			"Components": [
				{ "Class": "SceneComponent", "Name": "Root", "Parent": "", "LocalPosition": [0, 0, 0] },
				{ "Class": "ModelComponent", "Name": "Mesh", "Visible": true, "Material": { "Roughness": 0.5 } }
			]
		})json");
		const nlohmann::json instance = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Name": "EliteEnemy",
			"Tags": { "Team": "Enemy", "Boss": false },
			"Components": [
				{ "Class": "SceneComponent", "Name": "Root", "Parent": "", "LocalPosition": [3, 0, 1] },
				{ "Class": "AudioComponent", "Name": "AlertAudio", "Volume": 0.8 }
			]
		})json");

		const auto diff = EditorPrefabDiff::Build(base, instance);
		assert(diff.summary.actorPropertyChanges == 1);
		assert(diff.summary.componentAdded == 1);
		assert(diff.summary.componentRemoved == 1);
		assert(diff.summary.componentPropertyChanges == 1);
		assert(diff.summary.GetTotalChangeCount() == 4);
		assert(HasEntry(diff.entries, EditorPrefabDiffKind::ActorPropertyChanged, "", "Name"));
		assert(HasEntry(diff.entries, EditorPrefabDiffKind::ComponentAdded, "AlertAudio", ""));
		assert(HasEntry(diff.entries, EditorPrefabDiffKind::ComponentRemoved, "Mesh", ""));
		assert(HasEntry(diff.entries, EditorPrefabDiffKind::ComponentPropertyChanged, "Root", "LocalPosition"));
	}

	void TestComponentOrderDoesNotCreateFalseDiff()
	{
		const nlohmann::json base = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Components": [
				{ "Class": "SceneComponent", "Name": "Root", "Parent": "" },
				{ "Class": "ModelComponent", "Name": "Mesh", "Visible": true }
			]
		})json");
		const nlohmann::json reordered = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Components": [
				{ "Class": "ModelComponent", "Name": "Mesh", "Visible": true },
				{ "Class": "SceneComponent", "Name": "Root", "Parent": "" }
			]
		})json");

		const auto diff = EditorPrefabDiff::Build(base, reordered);
		assert(!diff.HasChanges()); // 配列順だけの差ではPrefab Overrideを表示しない。
	}

	void TestNestedPropertyRemovalTracksExistence()
	{
		const nlohmann::json base = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Components": [
				{ "Class": "LightComponent", "Name": "Light", "Settings": { "Intensity": 4.0, "CastShadow": true } }
			]
		})json");
		const nlohmann::json instance = nlohmann::json::parse(R"json(
		{
			"Class": "Actor",
			"Components": [
				{ "Class": "LightComponent", "Name": "Light", "Settings": { "Intensity": 4.0 } }
			]
		})json");

		const auto diff = EditorPrefabDiff::Build(base, instance);
		assert(diff.entries.size() == 1);
		const EditorPrefabDiffEntry& entry = diff.entries.front();
		assert(entry.kind == EditorPrefabDiffKind::ComponentPropertyChanged);
		assert(entry.componentName == "Light");
		assert(entry.propertyPath == "Settings.CastShadow");
		assert(entry.baseExists);
		assert(!entry.instanceExists);
		assert(entry.baseValue == true);
	}

	void TestSameNameClassReplacementIsStructural()
	{
		const nlohmann::json base = nlohmann::json::parse(R"json(
		{ "Class": "Actor", "Components": [ { "Class": "ModelComponent", "Name": "Visual" } ] }
		)json");
		const nlohmann::json instance = nlohmann::json::parse(R"json(
		{ "Class": "Actor", "Components": [ { "Class": "SpriteComponent", "Name": "Visual" } ] }
		)json");

		const auto diff = EditorPrefabDiff::Build(base, instance);
		assert(diff.summary.componentRemoved == 1);
		assert(diff.summary.componentAdded == 1);
		assert(diff.summary.componentPropertyChanges == 0);
	}
}

int main()
{
	TestSemanticActorAndComponentDiff();
	TestComponentOrderDoesNotCreateFalseDiff();
	TestNestedPropertyRemovalTracksExistence();
	TestSameNameClassReplacementIsStructural();
	std::cout << "Editor Prefab Diff runtime tests passed\n";
	return 0;
}
