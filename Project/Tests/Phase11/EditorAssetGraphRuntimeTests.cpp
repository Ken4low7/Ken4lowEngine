#include <EditorAssetGraph.h>

#include <cassert>
#include <iostream>

using namespace Ken4lowEngine;

namespace
{
	nlohmann::json MakeAsset(
		const char* id,
		const char* type,
		const char* logicalKey,
		const char* metaPath,
		std::initializer_list<const char*> dependencies,
		std::initializer_list<const char*> outputs)
	{
		nlohmann::json dependencyJson = nlohmann::json::array();
		for (const char* path : dependencies) dependencyJson.push_back({ { "Path", path } });
		nlohmann::json outputJson = nlohmann::json::array();
		for (const char* path : outputs) outputJson.push_back(path);
		return {
			{ "AssetId", id },
			{ "AssetType", type },
			{ "LogicalKey", logicalKey },
			{ "BuildKey", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" },
			{ "MetaPath", metaPath },
			{ "Dependencies", dependencyJson },
			{ "OutputPaths", outputJson },
			{ "MissingOutputs", nlohmann::json::array() },
		};
	}
}

int main()
{
	const nlohmann::json manifest = {
		{ "ManifestVersion", 1 },
		{ "Assets", nlohmann::json::array({
			MakeAsset(
				"aaaaaaaaaaaaaaaa",
				"Texture",
				"Resources/Textures/Sources/A.png",
				"Resources/Textures/Compiled/A.buildmeta.json",
				{ "Resources/Textures/Sources/A.png" },
				{ "Resources/Textures/Compiled/A.dds" }),
			MakeAsset(
				"bbbbbbbbbbbbbbbb",
				"Model",
				"Resources/Models/Sources/B.gltf#static",
				"Resources/Models/Compiled/B.buildmeta.json",
				{ "Resources/Models/Sources/B.gltf", "Resources/Textures/Compiled/A.dds" },
				{ "Resources/Models/Compiled/B.kmesh" }),
			MakeAsset(
				"cccccccccccccccc",
				"Model",
				"Resources/Models/Sources/C.gltf",
				"Resources/Models/Compiled/C.buildmeta.json",
				{ "Resources/Models/Sources/C.gltf", "Resources/Models/Compiled/B.kmesh" },
				{ "Resources/Models/Compiled/C.kmesh" }),
			MakeAsset(
				"dddddddddddddddd",
				"Texture",
				"Resources/Textures/Sources/Unrelated.png",
				"Resources/Textures/Compiled/Unrelated.buildmeta.json",
				{ "Resources/Textures/Sources/Unrelated.png" },
				{ "Resources/Textures/Compiled/Unrelated.dds" }),
		}) },
	};

	const nlohmann::json packageManifest = {
		{ "PackageVersion", 1 },
		{ "AssetToChunk", {
			{ "aaaaaaaaaaaaaaaa", "ui" },
			{ "bbbbbbbbbbbbbbbb", "world" },
			{ "cccccccccccccccc", "world" },
			{ "dddddddddddddddd", "core" },
		} },
	};

	EditorAssetGraph graph;
	assert(graph.LoadFromJson(manifest, packageManifest));
	assert(graph.IsLoaded());
	assert(graph.IsPackageManifestLoaded());
	assert(graph.GetAssets().size() == 4);

	// Source change rebuilds its own cook record and propagates through produced outputs transitively.
	const EditorAssetGraphSelection sourceSelection = graph.BuildSelection("resources\\textures\\sources\\A.png");
	assert(sourceSelection.matchedAssetIds.size() == 1);
	assert(sourceSelection.matchedAssetIds.front() == "aaaaaaaaaaaaaaaa");
	assert(sourceSelection.directDependentAssetIds.size() == 1);
	assert(sourceSelection.directDependentAssetIds.front() == "bbbbbbbbbbbbbbbb");
	assert(sourceSelection.affectedAssetIds.size() == 3);
	assert(sourceSelection.affectedAssetIds[0] == "aaaaaaaaaaaaaaaa");
	assert(sourceSelection.affectedAssetIds[1] == "bbbbbbbbbbbbbbbb");
	assert(sourceSelection.affectedAssetIds[2] == "cccccccccccccccc");
	assert(sourceSelection.affectedChunkIds.size() == 2);
	assert(sourceSelection.affectedChunkIds[0] == "ui");
	assert(sourceSelection.affectedChunkIds[1] == "world");

	const EditorAssetGraphSelection modelSelection = graph.BuildSelection("Resources/Models/Sources/B.gltf");
	assert(modelSelection.matchedAssetIds.size() == 1); // Variant suffix does not prevent source-path selection.
	assert(modelSelection.dependencyPaths.size() == 2);
	assert(modelSelection.directDependentAssetIds.size() == 1);
	assert(modelSelection.directDependentAssetIds.front() == "cccccccccccccccc");

	const EditorAssetGraphSelection outputSelection = graph.BuildSelection("Resources/Textures/Compiled/A.dds");
	assert(outputSelection.matchedAssetIds.size() == 1); // Producer is inspectable from a cooked output selection.
	assert(outputSelection.affectedAssetIds.size() == 2);
	assert(outputSelection.affectedAssetIds[0] == "bbbbbbbbbbbbbbbb");
	assert(outputSelection.affectedAssetIds[1] == "cccccccccccccccc");

	const EditorAssetGraphSelection unrelated = graph.BuildSelection("Resources/Textures/Sources/Unrelated.png");
	assert(unrelated.affectedAssetIds.size() == 1);
	assert(unrelated.affectedChunkIds.size() == 1 && unrelated.affectedChunkIds.front() == "core");

	const EditorAssetGraphSelection unknown = graph.BuildSelection("Resources/ActorPrefabs/NotCooked.json");
	assert(!unknown.HasRelations());

	const nlohmann::json invalidManifest = { { "ManifestVersion", 1 } };
	assert(!graph.LoadFromJson(invalidManifest));
	assert(!graph.IsLoaded()); // Failed reload must not expose stale relationships from the previous manifest.
	assert(graph.GetAssets().empty());

	std::cout << "Editor Asset Graph runtime tests passed\n";
	return 0;
}
