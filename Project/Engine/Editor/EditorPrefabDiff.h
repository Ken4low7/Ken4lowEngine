#pragma once

#include <json.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	enum class EditorPrefabDiffKind
	{
		ActorPropertyChanged,
		ComponentAdded,
		ComponentRemoved,
		ComponentPropertyChanged,
	};

	struct EditorPrefabDiffEntry final
	{
		EditorPrefabDiffKind kind = EditorPrefabDiffKind::ActorPropertyChanged;
		std::string componentKey;
		std::string componentName;
		std::string componentClass;
		std::string propertyPath;
		bool baseExists = false;
		bool instanceExists = false;
		nlohmann::json baseValue;
		nlohmann::json instanceValue;
	};

	struct EditorPrefabDiffSummary final
	{
		std::size_t actorPropertyChanges = 0;
		std::size_t componentAdded = 0;
		std::size_t componentRemoved = 0;
		std::size_t componentPropertyChanges = 0;

		[[nodiscard]] std::size_t GetTotalChangeCount() const
		{
			return actorPropertyChanges + componentAdded + componentRemoved + componentPropertyChanges;
		}
	};

	struct EditorPrefabDiffResult final
	{
		EditorPrefabDiffSummary summary{};
		std::vector<EditorPrefabDiffEntry> entries;

		[[nodiscard]] bool HasChanges() const { return !entries.empty(); }
	};

	/// Prefab baseとInstance Actor JSONをComponent identity単位で比較し、Editor表示用の決定的な差分を生成する。
	class EditorPrefabDiff final
	{
	public:
		static EditorPrefabDiffResult Build(const nlohmann::json& baseActorJson, const nlohmann::json& instanceActorJson)
		{
			EditorPrefabDiffResult result{};
			CompareActorProperties(baseActorJson, instanceActorJson, result);
			CompareComponents(baseActorJson, instanceActorJson, result);

			std::sort(result.entries.begin(), result.entries.end(), [](const EditorPrefabDiffEntry& lhs, const EditorPrefabDiffEntry& rhs)
				{
					const int lhsRank = KindSortRank(lhs.kind);
					const int rhsRank = KindSortRank(rhs.kind);
					if (lhsRank != rhsRank) return lhsRank < rhsRank;
					if (lhs.componentKey != rhs.componentKey) return lhs.componentKey < rhs.componentKey;
					return lhs.propertyPath < rhs.propertyPath;
				});
			return result;
		}

		static std::string_view ToString(EditorPrefabDiffKind kind)
		{
			switch (kind)
			{
			case EditorPrefabDiffKind::ActorPropertyChanged: return "Actor Property";
			case EditorPrefabDiffKind::ComponentAdded: return "Component Added";
			case EditorPrefabDiffKind::ComponentRemoved: return "Component Removed";
			case EditorPrefabDiffKind::ComponentPropertyChanged: return "Component Property";
			default: return "Unknown";
			}
		}

	private:
		struct ComponentRecord final
		{
			std::string key;
			std::string name;
			std::string className;
			nlohmann::json value;
		};

		using ComponentMap = std::map<std::string, ComponentRecord>;

		static int KindSortRank(EditorPrefabDiffKind kind)
		{
			switch (kind)
			{
			case EditorPrefabDiffKind::ActorPropertyChanged: return 0;
			case EditorPrefabDiffKind::ComponentAdded: return 1;
			case EditorPrefabDiffKind::ComponentRemoved: return 2;
			case EditorPrefabDiffKind::ComponentPropertyChanged: return 3;
			default: return 4;
			}
		}

		static std::string JoinPath(std::string_view parent, std::string_view child)
		{
			if (parent.empty()) return std::string(child);
			if (child.empty()) return std::string(parent);
			return std::string(parent) + "." + std::string(child);
		}

		static void CountEntry(EditorPrefabDiffKind kind, EditorPrefabDiffSummary& summary)
		{
			switch (kind)
			{
			case EditorPrefabDiffKind::ActorPropertyChanged: ++summary.actorPropertyChanges; break;
			case EditorPrefabDiffKind::ComponentAdded: ++summary.componentAdded; break;
			case EditorPrefabDiffKind::ComponentRemoved: ++summary.componentRemoved; break;
			case EditorPrefabDiffKind::ComponentPropertyChanged: ++summary.componentPropertyChanges; break;
			default: break;
			}
		}

		static void AddValueEntry(
			EditorPrefabDiffResult& result,
			EditorPrefabDiffKind kind,
			std::string componentKey,
			std::string componentName,
			std::string componentClass,
			std::string propertyPath,
			const nlohmann::json* baseValue,
			const nlohmann::json* instanceValue)
		{
			EditorPrefabDiffEntry entry{};
			entry.kind = kind;
			entry.componentKey = std::move(componentKey);
			entry.componentName = std::move(componentName);
			entry.componentClass = std::move(componentClass);
			entry.propertyPath = std::move(propertyPath);
			entry.baseExists = baseValue != nullptr;
			entry.instanceExists = instanceValue != nullptr;
			if (baseValue) entry.baseValue = *baseValue;
			if (instanceValue) entry.instanceValue = *instanceValue;
			result.entries.push_back(std::move(entry));
			CountEntry(kind, result.summary);
		}

		static void CompareValue(
			const nlohmann::json* baseValue,
			const nlohmann::json* instanceValue,
			std::string_view path,
			EditorPrefabDiffKind kind,
			std::string_view componentKey,
			std::string_view componentName,
			std::string_view componentClass,
			EditorPrefabDiffResult& result)
		{
			if (baseValue && instanceValue && *baseValue == *instanceValue) return;

			if (baseValue && instanceValue && baseValue->is_object() && instanceValue->is_object())
			{
				std::set<std::string> keys;
				for (auto it = baseValue->begin(); it != baseValue->end(); ++it) keys.insert(it.key());
				for (auto it = instanceValue->begin(); it != instanceValue->end(); ++it) keys.insert(it.key());
				for (const std::string& key : keys)
				{
					const auto baseIt = baseValue->find(key);
					const auto instanceIt = instanceValue->find(key);
					const nlohmann::json* childBase = baseIt != baseValue->end() ? &baseIt.value() : nullptr;
					const nlohmann::json* childInstance = instanceIt != instanceValue->end() ? &instanceIt.value() : nullptr;
					CompareValue(
						childBase,
						childInstance,
						JoinPath(path, key),
						kind,
						componentKey,
						componentName,
						componentClass,
						result);
				}
				return;
			}

			AddValueEntry(
				result,
				kind,
				std::string(componentKey),
				std::string(componentName),
				std::string(componentClass),
				std::string(path),
				baseValue,
				instanceValue);
		}

		static void CompareActorProperties(
			const nlohmann::json& baseActorJson,
			const nlohmann::json& instanceActorJson,
			EditorPrefabDiffResult& result)
		{
			std::set<std::string> keys;
			if (baseActorJson.is_object())
			{
				for (auto it = baseActorJson.begin(); it != baseActorJson.end(); ++it)
				{
					if (it.key() != "Components") keys.insert(it.key());
				}
			}
			if (instanceActorJson.is_object())
			{
				for (auto it = instanceActorJson.begin(); it != instanceActorJson.end(); ++it)
				{
					if (it.key() != "Components") keys.insert(it.key());
				}
			}

			for (const std::string& key : keys)
			{
				const nlohmann::json* baseValue = nullptr;
				const nlohmann::json* instanceValue = nullptr;
				if (baseActorJson.is_object())
				{
					const auto found = baseActorJson.find(key);
					if (found != baseActorJson.end()) baseValue = &found.value();
				}
				if (instanceActorJson.is_object())
				{
					const auto found = instanceActorJson.find(key);
					if (found != instanceActorJson.end()) instanceValue = &found.value();
				}
				CompareValue(baseValue, instanceValue, key, EditorPrefabDiffKind::ActorPropertyChanged, {}, {}, {}, result);
			}
		}

		static ComponentMap CollectComponents(const nlohmann::json& actorJson)
		{
			ComponentMap components;
			if (!actorJson.is_object() || !actorJson.contains("Components") || !actorJson["Components"].is_array()) return components;

			std::map<std::string, std::size_t> occurrences;
			for (const nlohmann::json& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object()) continue;
				const std::string name = componentJson.value("Name", std::string{});
				const std::string className = componentJson.value("Class", std::string{});
				const std::string identityBase = !name.empty() ? "N:" + name : "C:" + className;
				const std::size_t occurrence = occurrences[identityBase]++;
				const std::string key = occurrence == 0 ? identityBase : identityBase + "#" + std::to_string(occurrence);
				const std::string displayName = !name.empty()
					? name
					: (className.empty() ? "<unnamed>" : className + " #" + std::to_string(occurrence + 1));
				components.emplace(key, ComponentRecord{ key, displayName, className, componentJson });
			}
			return components;
		}

		static void AddComponentChange(
			EditorPrefabDiffResult& result,
			EditorPrefabDiffKind kind,
			const ComponentRecord& component,
			bool isBase)
		{
			const nlohmann::json* baseValue = isBase ? &component.value : nullptr;
			const nlohmann::json* instanceValue = isBase ? nullptr : &component.value;
			AddValueEntry(
				result,
				kind,
				component.key,
				component.name,
				component.className,
				{},
				baseValue,
				instanceValue);
		}

		static void CompareComponentProperties(
			const ComponentRecord& baseComponent,
			const ComponentRecord& instanceComponent,
			EditorPrefabDiffResult& result)
		{
			std::set<std::string> keys;
			for (auto it = baseComponent.value.begin(); it != baseComponent.value.end(); ++it)
			{
				if (it.key() != "Name" && it.key() != "Class") keys.insert(it.key());
			}
			for (auto it = instanceComponent.value.begin(); it != instanceComponent.value.end(); ++it)
			{
				if (it.key() != "Name" && it.key() != "Class") keys.insert(it.key());
			}

			for (const std::string& key : keys)
			{
				const auto baseIt = baseComponent.value.find(key);
				const auto instanceIt = instanceComponent.value.find(key);
				const nlohmann::json* baseValue = baseIt != baseComponent.value.end() ? &baseIt.value() : nullptr;
				const nlohmann::json* instanceValue = instanceIt != instanceComponent.value.end() ? &instanceIt.value() : nullptr;
				CompareValue(
					baseValue,
					instanceValue,
					key,
					EditorPrefabDiffKind::ComponentPropertyChanged,
					baseComponent.key,
					baseComponent.name,
					baseComponent.className,
					result);
			}
		}

		static void CompareComponents(
			const nlohmann::json& baseActorJson,
			const nlohmann::json& instanceActorJson,
			EditorPrefabDiffResult& result)
		{
			const ComponentMap baseComponents = CollectComponents(baseActorJson);
			const ComponentMap instanceComponents = CollectComponents(instanceActorJson);

			for (const auto& [key, baseComponent] : baseComponents)
			{
				const auto instanceIt = instanceComponents.find(key);
				if (instanceIt == instanceComponents.end())
				{
					AddComponentChange(result, EditorPrefabDiffKind::ComponentRemoved, baseComponent, true);
					continue;
				}

				const ComponentRecord& instanceComponent = instanceIt->second;
				if (baseComponent.className != instanceComponent.className)
				{
					// 同名ComponentのClass差し替えはProperty変更ではなくRemove+Addとして扱う。
					AddComponentChange(result, EditorPrefabDiffKind::ComponentRemoved, baseComponent, true);
					AddComponentChange(result, EditorPrefabDiffKind::ComponentAdded, instanceComponent, false);
					continue;
				}
				CompareComponentProperties(baseComponent, instanceComponent, result);
			}

			for (const auto& [key, instanceComponent] : instanceComponents)
			{
				if (baseComponents.contains(key)) continue;
				AddComponentChange(result, EditorPrefabDiffKind::ComponentAdded, instanceComponent, false);
			}
		}
	};
} // namespace Ken4lowEngine
