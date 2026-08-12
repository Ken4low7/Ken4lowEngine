#pragma once

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	struct EditorAssetGraphAsset final
	{
		std::string assetId;
		std::string assetType;
		std::string logicalKey;
		std::string buildKey;
		std::string metaPath;
		std::vector<std::string> dependencies;
		std::vector<std::string> outputPaths;
		std::vector<std::string> missingOutputs;
		std::string chunkId;
	};

	struct EditorAssetGraphSelection final
	{
		std::string selectedPath;
		std::vector<std::string> matchedAssetIds;
		std::vector<std::string> dependencyPaths;
		std::vector<std::string> directDependentAssetIds;
		std::vector<std::string> affectedAssetIds;
		std::vector<std::string> affectedChunkIds;

		[[nodiscard]] bool HasRelations() const
		{
			return !matchedAssetIds.empty() || !directDependentAssetIds.empty() || !affectedAssetIds.empty();
		}
	};

	/// Phase 8 AssetManifest/PackageManifestを読み取り、Editor用の双方向依存・再build影響範囲を構築する。
	class EditorAssetGraph final
	{
	public:
		bool Load(const std::filesystem::path& projectDirectory)
		{
			Clear();
			projectDirectory_ = projectDirectory;
			manifestPath_ = projectDirectory_.parent_path() / "Generated" / "AssetPipeline" / "AssetManifest.json";
			packageManifestPath_ = projectDirectory_.parent_path() / "Generated" / "Packages" / "PackageManifest.json";

			nlohmann::json manifest;
			if (!ReadJsonFile(manifestPath_, manifest, lastError_)) return false;

			nlohmann::json packageManifest = nlohmann::json::object();
			std::string packageError;
			if (ReadJsonFile(packageManifestPath_, packageManifest, packageError))
			{
				packageManifestLoaded_ = true;
			}
			else
			{
				packageManifestLoaded_ = false; // Package未生成でもAsset dependency graph自体は利用可能にする。
			}

			if (!LoadFromJson(manifest, packageManifest)) return false;
			lastError_.clear();
			return true;
		}

		bool LoadFromJson(const nlohmann::json& manifest, const nlohmann::json& packageManifest = nlohmann::json::object())
		{
			assets_.clear();
			assetIndexById_.clear();
			logicalAssetsByPath_.clear();
			outputAssetsByPath_.clear();
			metaAssetsByPath_.clear();
			consumersByDependencyPath_.clear();
			packageManifestLoaded_ = packageManifest.is_object() && packageManifest.contains("AssetToChunk");
			lastError_.clear();

			if (!manifest.is_object() || !manifest.contains("Assets") || !manifest["Assets"].is_array())
			{
				lastError_ = "AssetManifestのAssets配列が見つかりません。";
				return false;
			}

			std::unordered_map<std::string, std::string> chunkByAssetId;
			if (packageManifestLoaded_ && packageManifest["AssetToChunk"].is_object())
			{
				for (auto it = packageManifest["AssetToChunk"].begin(); it != packageManifest["AssetToChunk"].end(); ++it)
				{
					if (!it.value().is_string()) continue;
					chunkByAssetId[ToLower(it.key())] = it.value().get<std::string>();
				}
			}

			for (const nlohmann::json& assetJson : manifest["Assets"])
			{
				if (!assetJson.is_object()) continue;
				EditorAssetGraphAsset asset{};
				asset.assetId = ToLower(assetJson.value("AssetId", std::string{}));
				asset.assetType = assetJson.value("AssetType", std::string("Unknown"));
				asset.logicalKey = NormalizePath(assetJson.value("LogicalKey", std::string{}));
				asset.buildKey = ToLower(assetJson.value("BuildKey", std::string{}));
				asset.metaPath = NormalizePath(assetJson.value("MetaPath", std::string{}));
				if (asset.assetId.empty()) continue;

				for (const nlohmann::json& dependency : assetJson.value("Dependencies", nlohmann::json::array()))
				{
					if (!dependency.is_object() || !dependency.contains("Path") || !dependency["Path"].is_string()) continue;
					asset.dependencies.push_back(NormalizePath(dependency["Path"].get<std::string>()));
				}
				for (const nlohmann::json& output : assetJson.value("OutputPaths", nlohmann::json::array()))
				{
					if (output.is_string()) asset.outputPaths.push_back(NormalizePath(output.get<std::string>()));
				}
				for (const nlohmann::json& missing : assetJson.value("MissingOutputs", nlohmann::json::array()))
				{
					if (missing.is_string()) asset.missingOutputs.push_back(NormalizePath(missing.get<std::string>()));
				}

				SortUnique(asset.dependencies);
				SortUnique(asset.outputPaths);
				SortUnique(asset.missingOutputs);
				const auto chunk = chunkByAssetId.find(asset.assetId);
				if (chunk != chunkByAssetId.end()) asset.chunkId = chunk->second;
				assets_.push_back(std::move(asset));
			}

			std::sort(assets_.begin(), assets_.end(), [](const EditorAssetGraphAsset& lhs, const EditorAssetGraphAsset& rhs)
				{
					if (lhs.assetType != rhs.assetType) return lhs.assetType < rhs.assetType;
					if (lhs.logicalKey != rhs.logicalKey) return lhs.logicalKey < rhs.logicalKey;
					return lhs.assetId < rhs.assetId;
				});

			for (std::size_t index = 0; index < assets_.size(); ++index)
			{
				const EditorAssetGraphAsset& asset = assets_[index];
				if (!assetIndexById_.emplace(asset.assetId, index).second)
				{
					lastError_ = "AssetManifestに重複AssetIdがあります: " + asset.assetId;
					ClearIndexes();
					return false;
				}

				const std::string logicalBase = LogicalBasePath(asset.logicalKey);
				if (!logicalBase.empty()) logicalAssetsByPath_[PathKey(logicalBase)].push_back(asset.assetId);
				if (!asset.metaPath.empty()) metaAssetsByPath_[PathKey(asset.metaPath)].push_back(asset.assetId);
				for (const std::string& outputPath : asset.outputPaths)
				{
					outputAssetsByPath_[PathKey(outputPath)].push_back(asset.assetId);
				}
				for (const std::string& dependencyPath : asset.dependencies)
				{
					consumersByDependencyPath_[PathKey(dependencyPath)].push_back(asset.assetId);
				}
			}

			SortUniqueIndex(logicalAssetsByPath_);
			SortUniqueIndex(outputAssetsByPath_);
			SortUniqueIndex(metaAssetsByPath_);
			SortUniqueIndex(consumersByDependencyPath_);
			loaded_ = true;
			return true;
		}

		void Clear()
		{
			loaded_ = false;
			packageManifestLoaded_ = false;
			projectDirectory_.clear();
			manifestPath_.clear();
			packageManifestPath_.clear();
			lastError_.clear();
			assets_.clear();
			ClearIndexes();
		}

		[[nodiscard]] bool IsLoaded() const { return loaded_; }
		[[nodiscard]] bool IsPackageManifestLoaded() const { return packageManifestLoaded_; }
		[[nodiscard]] const std::string& GetLastError() const { return lastError_; }
		[[nodiscard]] const std::filesystem::path& GetManifestPath() const { return manifestPath_; }
		[[nodiscard]] const std::filesystem::path& GetPackageManifestPath() const { return packageManifestPath_; }
		[[nodiscard]] const std::vector<EditorAssetGraphAsset>& GetAssets() const { return assets_; }

		[[nodiscard]] const EditorAssetGraphAsset* FindAsset(std::string_view assetId) const
		{
			const auto found = assetIndexById_.find(ToLower(std::string(assetId)));
			return found == assetIndexById_.end() ? nullptr : &assets_[found->second];
		}

		[[nodiscard]] EditorAssetGraphSelection BuildSelection(std::string_view selectedPath) const
		{
			EditorAssetGraphSelection selection{};
			selection.selectedPath = NormalizePath(std::string(selectedPath));
			if (!loaded_ || selection.selectedPath.empty()) return selection;

			const std::string key = PathKey(selection.selectedPath);
			std::set<std::string> matched;
			AppendIndexValues(logicalAssetsByPath_, key, matched);
			AppendIndexValues(outputAssetsByPath_, key, matched);
			AppendIndexValues(metaAssetsByPath_, key, matched);
			selection.matchedAssetIds.assign(matched.begin(), matched.end());

			std::set<std::string> dependencies;
			for (const std::string& assetId : selection.matchedAssetIds)
			{
				if (const EditorAssetGraphAsset* asset = FindAsset(assetId))
				{
					dependencies.insert(asset->dependencies.begin(), asset->dependencies.end());
				}
			}
			selection.dependencyPaths.assign(dependencies.begin(), dependencies.end());

			std::set<std::string> directDependents;
			AppendIndexValues(consumersByDependencyPath_, key, directDependents);
			for (const std::string& assetId : selection.matchedAssetIds)
			{
				const EditorAssetGraphAsset* asset = FindAsset(assetId);
				if (!asset) continue;
				for (const std::string& outputPath : asset->outputPaths)
				{
					AppendIndexValues(consumersByDependencyPath_, PathKey(outputPath), directDependents);
				}
			}
			for (const std::string& assetId : selection.matchedAssetIds) directDependents.erase(assetId);
			selection.directDependentAssetIds.assign(directDependents.begin(), directDependents.end());

			std::set<std::string> affected;
			std::queue<std::string> pending;
			auto enqueue = [&affected, &pending](const std::string& assetId)
				{
					if (affected.insert(assetId).second) pending.push(assetId);
				};

			const bool selectedIsLogicalOrMeta = logicalAssetsByPath_.contains(key) || metaAssetsByPath_.contains(key);
			if (selectedIsLogicalOrMeta)
			{
				for (const std::string& assetId : selection.matchedAssetIds) enqueue(assetId);
			}
			const auto directConsumers = consumersByDependencyPath_.find(key);
			if (directConsumers != consumersByDependencyPath_.end())
			{
				for (const std::string& assetId : directConsumers->second) enqueue(assetId);
			}
			for (const std::string& assetId : selection.directDependentAssetIds) enqueue(assetId);

			while (!pending.empty())
			{
				const std::string currentId = pending.front();
				pending.pop();
				const EditorAssetGraphAsset* current = FindAsset(currentId);
				if (!current) continue;
				for (const std::string& outputPath : current->outputPaths)
				{
					const auto consumers = consumersByDependencyPath_.find(PathKey(outputPath));
					if (consumers == consumersByDependencyPath_.end()) continue;
					for (const std::string& consumerId : consumers->second) enqueue(consumerId);
				}
			}

			selection.affectedAssetIds.assign(affected.begin(), affected.end());
			std::set<std::string> chunks;
			for (const std::string& assetId : selection.affectedAssetIds)
			{
				const EditorAssetGraphAsset* asset = FindAsset(assetId);
				if (asset && !asset->chunkId.empty()) chunks.insert(asset->chunkId);
			}
			selection.affectedChunkIds.assign(chunks.begin(), chunks.end());
			return selection;
		}

		static std::string NormalizePath(std::string value)
		{
			std::replace(value.begin(), value.end(), '\\', '/');
			while (value.starts_with("./")) value.erase(0, 2);
			while (!value.empty() && value.front() == '/') value.erase(value.begin());
			return value;
		}

	private:
		using PathIndex = std::unordered_map<std::string, std::vector<std::string>>;

		static bool ReadJsonFile(const std::filesystem::path& path, nlohmann::json& outJson, std::string& outError)
		{
			try
			{
				std::ifstream file(path);
				if (!file.is_open())
				{
					outError = "JSONファイルを開けません: " + path.generic_string();
					return false;
				}
				file >> outJson;
				return true;
			}
			catch (const std::exception& exception)
			{
				outError = "JSON読込に失敗しました: " + path.generic_string() + " : " + exception.what();
				return false;
			}
		}

		static std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			return value;
		}

		static std::string PathKey(std::string_view path)
		{
			return ToLower(NormalizePath(std::string(path)));
		}

		static std::string LogicalBasePath(std::string_view logicalKey)
		{
			std::string value = NormalizePath(std::string(logicalKey));
			const std::size_t variant = value.find('#');
			if (variant != std::string::npos) value.resize(variant);
			return value;
		}

		static void SortUnique(std::vector<std::string>& values)
		{
			std::sort(values.begin(), values.end());
			values.erase(std::unique(values.begin(), values.end()), values.end());
		}

		static void SortUniqueIndex(PathIndex& index)
		{
			for (auto& [path, assetIds] : index)
			{
				(void)path;
				SortUnique(assetIds);
			}
		}

		static void AppendIndexValues(const PathIndex& index, const std::string& key, std::set<std::string>& target)
		{
			const auto found = index.find(key);
			if (found == index.end()) return;
			target.insert(found->second.begin(), found->second.end());
		}

		void ClearIndexes()
		{
			assetIndexById_.clear();
			logicalAssetsByPath_.clear();
			outputAssetsByPath_.clear();
			metaAssetsByPath_.clear();
			consumersByDependencyPath_.clear();
		}

		bool loaded_ = false;
		bool packageManifestLoaded_ = false;
		std::filesystem::path projectDirectory_;
		std::filesystem::path manifestPath_;
		std::filesystem::path packageManifestPath_;
		std::string lastError_;
		std::vector<EditorAssetGraphAsset> assets_;
		std::unordered_map<std::string, std::size_t> assetIndexById_;
		PathIndex logicalAssetsByPath_;
		PathIndex outputAssetsByPath_;
		PathIndex metaAssetsByPath_;
		PathIndex consumersByDependencyPath_;
	};
} // namespace Ken4lowEngine
