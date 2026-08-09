#pragma once

#include <BlenderSceneLoader.h>
#include <LevelData.h>
#include <LevelLoader.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace K4E = ::Ken4lowEngine;

/// DebugSceneだけで旧LevelDataと新BlenderSceneDataの読み込み結果を比較する診断。
class LevelDataValidation
{
public:
	struct LegacySummary
	{
		std::size_t objectCount = 0;
		std::size_t staticMeshCount = 0;
		std::size_t colliderCount = 0;
		std::size_t spawnPointCount = 0;
		std::size_t knownPropertyObjectCount = 0;
		bool parentHierarchyPreserved = false;
		bool arbitraryCustomPropertiesPreserved = false;
	};

	struct BlenderSummary
	{
		std::size_t rootObjectCount = 0;
		std::size_t totalObjectCount = 0;
		std::size_t hierarchyEdgeCount = 0;
		std::size_t maxDepth = 0;
		std::size_t staticMeshCount = 0;
		std::size_t colliderCount = 0;
		std::size_t customPropertyObjectCount = 0;
		std::size_t rawObjectCount = 0;
		bool parentHierarchyPreserved = false;
		bool arbitraryCustomPropertiesPreserved = false;
	};

	LevelDataValidation()
	{
		Load("stages/fps_stage00.json");
	}

	bool Load(const std::string& relativePath)
	{
		sourcePath_ = relativePath;
		legacySummary_ = {};
		blenderSummary_ = {};

		blenderLoadResult_ = K4E::BlenderSceneLoader::Load(relativePath);
		levelData_ = K4E::LevelLoader::LoadLevel(relativePath);

		if (!levelData_)
		{
			lastMessage_ = "旧LevelDataの読み込みに失敗しました。";
			std::cerr << "[LevelDataValidation] Failed to load legacy LevelData: " << sourcePath_ << '\n';
			return false;
		}

		for (const K4E::ObjectData& object : levelData_->objects)
		{
			++legacySummary_.objectCount;
			if (IsStaticMeshType(object.type)) ++legacySummary_.staticMeshCount;
			if (object.collider.enabled) ++legacySummary_.colliderCount;
			if (IsSpawnPointType(object.type)) ++legacySummary_.spawnPointCount;
			if (HasKnownProperties(object)) ++legacySummary_.knownPropertyObjectCount;
		}

		if (blenderLoadResult_.succeeded)
		{
			blenderSummary_.rootObjectCount = blenderLoadResult_.scene.objects.size();
			for (const K4E::BlenderObjectData& object : blenderLoadResult_.scene.objects)
			{
				AccumulateBlenderObject(object, 0);
			}
			blenderSummary_.parentHierarchyPreserved = true;
			blenderSummary_.arbitraryCustomPropertiesPreserved = true;
			lastMessage_ = "旧LevelDataとBlenderSceneDataの両方を読み込みました。";
		}
		else
		{
			lastMessage_ = blenderLoadResult_.message;
		}

		// 旧実行用データと新しいBlender入力データを同じJSONから比較できるよう診断結果を固定出力する。
		WriteSummaryToLog();
		return blenderLoadResult_.succeeded;
	}

	void DrawImGui()
	{
#ifdef USE_IMGUI
		if (!ImGui::Begin("LevelData 検証"))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("読み込みファイル: %s", sourcePath_.c_str());
		if (ImGui::Button("再読み込み"))
		{
			Load(sourcePath_);
		}
		ImGui::SameLine();
		ImGui::TextColored(
			blenderLoadResult_.succeeded ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.4f, 0.35f, 1.0f),
			"%s",
			lastMessage_.c_str());

		DrawComparisonSummary();
		DrawLegacyState();
		DrawBlenderState();

		if (levelData_)
		{
			DrawLegacyObjectPreview();
			DrawLegacySpecialObjectPreview();
		}
		if (blenderLoadResult_.succeeded)
		{
			DrawBlenderHierarchyPreview();
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	const LegacySummary& GetLegacySummary() const noexcept { return legacySummary_; }
	const BlenderSummary& GetBlenderSummary() const noexcept { return blenderSummary_; }
	const K4E::LevelData* GetLevelData() const noexcept { return levelData_.get(); }
	const K4E::BlenderSceneData* GetBlenderSceneData() const noexcept
	{
		return blenderLoadResult_.succeeded ? &blenderLoadResult_.scene : nullptr;
	}
	const std::string& GetSourcePath() const noexcept { return sourcePath_; }

private:
	static bool IsStaticMeshType(const std::string& type)
	{
		return type == "MESH" || type == "StaticMesh";
	}

	static bool IsSpawnPointType(const std::string& type)
	{
		return type == "PlayerSpawnPoint" ||
			type == "EnemySpawnPoint" ||
			type == "BossSpawnPoint";
	}

	static bool HasKnownProperties(const K4E::ObjectData& object)
	{
		return object.hasSpawnProps ||
			object.hasIntroCameraProps ||
			object.hasDeviceObjectiveProps ||
			object.hasDefenseTargetProps ||
			object.hasEscapePointProps ||
			object.hasBossPhaseTriggerProps;
	}

	void AccumulateBlenderObject(const K4E::BlenderObjectData& object, std::size_t depth)
	{
		++blenderSummary_.totalObjectCount;
		blenderSummary_.maxDepth = (std::max)(blenderSummary_.maxDepth, depth);
		blenderSummary_.hierarchyEdgeCount += object.children.size();
		if (IsStaticMeshType(object.type)) ++blenderSummary_.staticMeshCount;
		if (!object.collider.empty()) ++blenderSummary_.colliderCount;
		if (!object.properties.empty()) ++blenderSummary_.customPropertyObjectCount;
		if (object.raw.is_object()) ++blenderSummary_.rawObjectCount;

		for (const K4E::BlenderObjectData& child : object.children)
		{
			AccumulateBlenderObject(child, depth + 1);
		}
	}

#ifdef USE_IMGUI
	static void DrawSupportState(const char* label, bool supported)
	{
		ImGui::TextColored(
			supported ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f) : ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
			"%s: %s",
			label,
			supported ? "保持" : "未保持");
	}

	void DrawComparisonSummary() const
	{
		ImGui::SeparatorText("Phase 2 比較");
		if (ImGui::BeginTable("LevelDataPhase2Comparison", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("項目");
			ImGui::TableSetupColumn("旧 LevelData");
			ImGui::TableSetupColumn("新 BlenderSceneData");
			ImGui::TableHeadersRow();

			DrawComparisonRow("保存Object", legacySummary_.objectCount, blenderSummary_.totalObjectCount);
			DrawComparisonRow("StaticMesh", legacySummary_.staticMeshCount, blenderSummary_.staticMeshCount);
			DrawComparisonRow("Collider", legacySummary_.colliderCount, blenderSummary_.colliderCount);
			DrawComparisonRow("Properties", legacySummary_.knownPropertyObjectCount, blenderSummary_.customPropertyObjectCount);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("親子関係");
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("平坦化");
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("保持 / Edge=%zu", blenderSummary_.hierarchyEdgeCount);

			ImGui::EndTable();
		}
		ImGui::TextDisabled("Object数が一致しないのは、旧LevelDataが実行に必要な型だけを抽出して平坦化するためです。");
	}

	static void DrawComparisonRow(const char* label, std::size_t legacyValue, std::size_t blenderValue)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%zu", legacyValue);
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%zu", blenderValue);
	}

	void DrawLegacyState() const
	{
		ImGui::SeparatorText("旧 LevelData");
		ImGui::Text("Object: %zu", legacySummary_.objectCount);
		ImGui::Text("StaticMesh: %zu", legacySummary_.staticMeshCount);
		ImGui::Text("Collider: %zu", legacySummary_.colliderCount);
		ImGui::Text("SpawnPoint: %zu", legacySummary_.spawnPointCount);
		ImGui::Text("KnownProps: %zu", legacySummary_.knownPropertyObjectCount);
		DrawSupportState("親子関係", legacySummary_.parentHierarchyPreserved);
		DrawSupportState("任意Custom Properties", legacySummary_.arbitraryCustomPropertiesPreserved);
		ImGui::TextDisabled("親Transformは合成済みですが、親子関係そのものはLevelDataに保持されません。");
		ImGui::TextDisabled("C++側で定義済みのpropsだけが現在のLevelDataへ保存されます。");
	}

	void DrawBlenderState() const
	{
		ImGui::SeparatorText("新 BlenderSceneData");
		if (!blenderLoadResult_.succeeded)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s", blenderLoadResult_.message.c_str());
			return;
		}

		const K4E::BlenderSceneData& scene = blenderLoadResult_.scene;
		ImGui::Text("SchemaVersion: %d / Name: %s", scene.schemaVersion, scene.name.c_str());
		ImGui::Text("RootObject: %zu", blenderSummary_.rootObjectCount);
		ImGui::Text("TotalObject: %zu", blenderSummary_.totalObjectCount);
		ImGui::Text("Hierarchy Edge: %zu / MaxDepth: %zu", blenderSummary_.hierarchyEdgeCount, blenderSummary_.maxDepth);
		ImGui::Text("StaticMesh: %zu / Collider: %zu", blenderSummary_.staticMeshCount, blenderSummary_.colliderCount);
		ImGui::Text("Raw Custom Properties: %zu", blenderSummary_.customPropertyObjectCount);
		DrawSupportState("親子関係", blenderSummary_.parentHierarchyPreserved);
		DrawSupportState("任意Custom Properties", blenderSummary_.arbitraryCustomPropertiesPreserved);

		const K4E::BlenderSceneMetadata& meta = scene.metadata;
		ImGui::Text("Units: %s / Rotation: %s", meta.units.c_str(), meta.rotationUnit.c_str());
		ImGui::Text("Source: Forward=%s Up=%s", meta.sourceForward.c_str(), meta.sourceUp.c_str());
		ImGui::Text("Game: Forward=%s Up=%s", meta.gameForward.c_str(), meta.gameUp.c_str());
		ImGui::TextDisabled("TransformはBlender入力値のまま保持し、ゲーム座標への変換は次のImport段階へ分離しています。");
	}

	void DrawLegacyObjectPreview() const
	{
		constexpr std::size_t kPreviewCount = 10;
		const std::size_t previewCount = (std::min)(kPreviewCount, levelData_->objects.size());

		ImGui::SeparatorText("旧 LevelData Object Preview");
		if (ImGui::BeginTable("LevelDataObjectPreview", 6,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
			ImVec2(0.0f, 180.0f)))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Type");
			ImGui::TableSetupColumn("Model");
			ImGui::TableSetupColumn("Position");
			ImGui::TableSetupColumn("Rotation");
			ImGui::TableSetupColumn("Scale");
			ImGui::TableHeadersRow();

			for (std::size_t index = 0; index < previewCount; ++index)
			{
				const K4E::ObjectData& object = levelData_->objects[index];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(object.name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(object.type.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(object.modelName.c_str());
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.2f, %.2f, %.2f", object.position.x, object.position.y, object.position.z);
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%.2f, %.2f, %.2f", object.rotation.x, object.rotation.y, object.rotation.z);
				ImGui::TableSetColumnIndex(5);
				ImGui::Text("%.2f, %.2f, %.2f", object.scale.x, object.scale.y, object.scale.z);
			}
			ImGui::EndTable();
		}
	}

	void DrawLegacySpecialObjectPreview() const
	{
		constexpr std::size_t kPreviewCount = 10;
		std::size_t specialCount = 0;

		ImGui::SeparatorText("旧 Collider / Known Props Preview");
		for (const K4E::ObjectData& object : levelData_->objects)
		{
			if (!object.collider.enabled && !HasKnownProperties(object)) continue;

			ImGui::PushID(static_cast<int>(specialCount));
			if (ImGui::TreeNode("%s [%s]", object.name.c_str(), object.type.c_str()))
			{
				if (object.collider.enabled)
				{
					ImGui::Text("Collider Type: %s", object.collider.type.c_str());
					ImGui::Text("Collision Type: %s / Id: %d", object.collider.collisionType.c_str(), object.collider.collisionTypeId);
					ImGui::Text("Center: %.2f, %.2f, %.2f", object.collider.center.x, object.collider.center.y, object.collider.center.z);
					ImGui::Text("Size: %.2f, %.2f, %.2f", object.collider.size.x, object.collider.size.y, object.collider.size.z);
				}
				if (object.hasSpawnProps)
				{
					ImGui::Text("Spawn: Wave=%d Group=%d Count=%d", object.spawnProps.wave, object.spawnProps.group, object.spawnProps.count);
					ImGui::Text("Archetype=%s EnemyType=%s", object.spawnProps.archetype.c_str(), object.spawnProps.enemyType.c_str());
				}
				if (object.hasIntroCameraProps) ImGui::Text("IntroCamera Props: あり");
				if (object.hasDeviceObjectiveProps) ImGui::Text("DeviceObjective Props: あり");
				if (object.hasDefenseTargetProps) ImGui::Text("DefenseTarget Props: あり");
				if (object.hasEscapePointProps) ImGui::Text("EscapePoint Props: あり");
				if (object.hasBossPhaseTriggerProps) ImGui::Text("BossPhaseTrigger Props: あり");
				ImGui::TreePop();
			}
			ImGui::PopID();

			if (++specialCount >= kPreviewCount) break;
		}
	}

	void DrawBlenderHierarchyPreview() const
	{
		ImGui::SeparatorText("新 BlenderSceneData Hierarchy Preview");
		ImGui::BeginChild("BlenderSceneHierarchyPreview", ImVec2(0.0f, 320.0f), true);
		for (const K4E::BlenderObjectData& object : blenderLoadResult_.scene.objects)
		{
			DrawBlenderObjectNode(object);
		}
		ImGui::EndChild();
	}

	void DrawBlenderObjectNode(const K4E::BlenderObjectData& object) const
	{
		ImGui::PushID(&object);
		const ImGuiTreeNodeFlags flags = object.children.empty()
			? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth
			: ImGuiTreeNodeFlags_SpanAvailWidth;
		const bool open = ImGui::TreeNodeEx(
			"##BlenderObject",
			flags,
			"%s [%s]  Children=%zu Props=%zu",
			object.name.c_str(),
			object.type.c_str(),
			object.children.size(),
			object.properties.size());

		if (open && !object.children.empty())
		{
			ImGui::Text("Source T: %.2f, %.2f, %.2f", object.transform.translation.x, object.transform.translation.y, object.transform.translation.z);
			ImGui::Text("Source R: %.2f, %.2f, %.2f", object.transform.rotation.x, object.transform.rotation.y, object.transform.rotation.z);
			ImGui::Text("Source S: %.2f, %.2f, %.2f", object.transform.scaling.x, object.transform.scaling.y, object.transform.scaling.z);
			DrawBlenderProperties(object);
			for (const K4E::BlenderObjectData& child : object.children)
			{
				DrawBlenderObjectNode(child);
			}
			ImGui::TreePop();
		}
		else if (open)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("T(%.1f, %.1f, %.1f)", object.transform.translation.x, object.transform.translation.y, object.transform.translation.z);
		}
		ImGui::PopID();
	}

	static void DrawBlenderProperties(const K4E::BlenderObjectData& object)
	{
		if (object.properties.empty()) return;
		if (!ImGui::TreeNode("Raw Custom Properties")) return;

		for (auto iterator = object.properties.begin(); iterator != object.properties.end(); ++iterator)
		{
			const std::string value = iterator.value().dump();
			ImGui::TextWrapped("%s = %s", iterator.key().c_str(), value.c_str());
		}
		ImGui::TreePop();
	}
#endif // USE_IMGUI

	void WriteSummaryToLog() const
	{
		std::clog
			<< "[LevelDataValidation] Source=" << sourcePath_ << '\n'
			<< "  Legacy Object=" << legacySummary_.objectCount
			<< " StaticMesh=" << legacySummary_.staticMeshCount
			<< " Collider=" << legacySummary_.colliderCount
			<< " SpawnPoint=" << legacySummary_.spawnPointCount
			<< " KnownProps=" << legacySummary_.knownPropertyObjectCount << '\n';

		if (blenderLoadResult_.succeeded)
		{
			std::clog
				<< "  Blender Root=" << blenderSummary_.rootObjectCount
				<< " Total=" << blenderSummary_.totalObjectCount
				<< " HierarchyEdge=" << blenderSummary_.hierarchyEdgeCount
				<< " MaxDepth=" << blenderSummary_.maxDepth
				<< " StaticMesh=" << blenderSummary_.staticMeshCount
				<< " Collider=" << blenderSummary_.colliderCount
				<< " RawProps=" << blenderSummary_.customPropertyObjectCount << '\n';
		}
		else
		{
			std::clog << "  BlenderSceneData LoadFailed=" << blenderLoadResult_.message << '\n';
		}
	}

private:
	std::string sourcePath_;
	std::string lastMessage_;
	std::unique_ptr<K4E::LevelData> levelData_;
	K4E::BlenderSceneLoader::Result blenderLoadResult_;
	LegacySummary legacySummary_;
	BlenderSummary blenderSummary_;
};
