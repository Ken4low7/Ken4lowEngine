#define NOMINMAX
#include "StageInstancingManager.h"
#include "MineHiddenArenaLayout.h"
#include "CollapsedCityLayout.h"
#include "CollapsedCityPassageLayout.h"
#include "CentralControlColosseumLayout.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsStageMeshType(const std::string& type)
		{
			return type == "StaticMesh" || type == "MESH";
		}

		bool ContainsIgnoreCase(const std::string& text, const std::string& token)
		{
			std::string loweredText = text;
			std::string loweredToken = token;
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::transform(loweredToken.begin(), loweredToken.end(), loweredToken.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return loweredText.find(loweredToken) != std::string::npos;
		}

		Vector4 ResolveStageInstanceColor(const ObjectData& data)
		{
			if (ContainsIgnoreCase(data.name, "controlglow")) return { 0.12f, 0.72f, 0.96f, 1.0f };
			if (ContainsIgnoreCase(data.name, "controlpanel")) return { 0.92f, 0.58f, 0.14f, 1.0f };
			if (ContainsIgnoreCase(data.name, "controlpylon") || ContainsIgnoreCase(data.name, "controlupperring")) return { 0.19f, 0.25f, 0.31f, 1.0f };
			if (ContainsIgnoreCase(data.name, "controlarenafloor") || ContainsIgnoreCase(data.name, "controlarenadais")) return { 0.25f, 0.27f, 0.30f, 1.0f };
			if (ContainsIgnoreCase(data.name, "colosseumseat")) return { 0.39f, 0.34f, 0.29f, 1.0f };
			if (ContainsIgnoreCase(data.name, "colosseumcolumn") || ContainsIgnoreCase(data.name, "colosseumcapital") ||
				ContainsIgnoreCase(data.name, "colosseumarc") || ContainsIgnoreCase(data.name, "colosseumcrown"))
			{
				return { 0.48f, 0.44f, 0.38f, 1.0f };
			}
			if (ContainsIgnoreCase(data.name, "colosseumouterwall") || ContainsIgnoreCase(data.name, "colosseumimperial") || ContainsIgnoreCase(data.name, "colosseumentry"))
			{
				return { 0.34f, 0.31f, 0.29f, 1.0f }; // 古代石造の外観へ制御塔の寒色発光を重ね、最終Stageだけの材質差を出す。
			}
			if (ContainsIgnoreCase(data.name, "cityroad")) return { 0.13f, 0.14f, 0.16f, 1.0f };
			if (ContainsIgnoreCase(data.name, "citysinkhole")) return { 0.07f, 0.075f, 0.085f, 1.0f };
			if (ContainsIgnoreCase(data.name, "citybuswreck")) return { 0.38f, 0.22f, 0.12f, 1.0f };
			if (ContainsIgnoreCase(data.name, "citymonument")) return { 0.38f, 0.37f, 0.35f, 1.0f };
			if (ContainsIgnoreCase(data.name, "cityevac")) return { 0.18f, 0.42f, 0.54f, 1.0f };
			if (ContainsIgnoreCase(data.name, "citybuilding") || ContainsIgnoreCase(data.name, "cityfacade") ||
				ContainsIgnoreCase(data.name, "cityboundary") || ContainsIgnoreCase(data.name, "cityfreeway"))
			{
				return { 0.27f, 0.29f, 0.33f, 1.0f }; // 崩落都市のコンクリート群は寒色寄りに統一し、道路・瓦礫との材質差を残す。
			}
			if (ContainsIgnoreCase(data.name, "defensehazard")) return { 0.90f, 0.58f, 0.10f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensecore")) return { 0.18f, 0.62f, 0.82f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensetower") || ContainsIgnoreCase(data.name, "defensegatepylon")) return { 0.24f, 0.30f, 0.36f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defenseouterwall") || ContainsIgnoreCase(data.name, "defenseinnerwall")) return { 0.31f, 0.36f, 0.43f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensesupply")) return { 0.42f, 0.42f, 0.25f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensegenerator")) return { 0.26f, 0.39f, 0.48f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensebarricade")) return { 0.44f, 0.36f, 0.27f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensecover")) return { 0.35f, 0.40f, 0.45f, 1.0f };
			if (ContainsIgnoreCase(data.name, "defensefloor") || ContainsIgnoreCase(data.name, "defensecommandplatform") || ContainsIgnoreCase(data.name, "defenseapproach"))
			{
				return { 0.27f, 0.30f, 0.33f, 1.0f }; // 防衛拠点は役割ごとに色を分け、同一モデルでも戦場の構造を読み取りやすくする。
			}
			if (ContainsIgnoreCase(data.name, "hiddenpassageguide")) return { 0.12f, 0.42f, 0.72f, 1.0f };
			if (ContainsIgnoreCase(data.name, "hiddenpassage")) return { 0.18f, 0.19f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "hiddensupport") || ContainsIgnoreCase(data.name, "hiddengate")) return { 0.23f, 0.21f, 0.18f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domearena")) return { 0.22f, 0.21f, 0.19f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domewall") || ContainsIgnoreCase(data.name, "domeentry")) return { 0.19f, 0.18f, 0.17f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domering") || ContainsIgnoreCase(data.name, "domerib")) return { 0.12f, 0.16f, 0.19f, 1.0f };
			if (ContainsIgnoreCase(data.name, "domerockseal")) return { 0.25f, 0.23f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "strip")) return { 0.22f, 0.64f, 0.72f, 1.0f };
			if (ContainsIgnoreCase(data.name, "rockdetail")) return { 0.24f, 0.23f, 0.22f, 1.0f };
			if (ContainsIgnoreCase(data.name, "rubble")) return { 0.36f, 0.30f, 0.24f, 1.0f };
			if (ContainsIgnoreCase(data.name, "brokenbeam")) return { 0.25f, 0.21f, 0.18f, 1.0f };
			if (ContainsIgnoreCase(data.name, "raisedfloor") || ContainsIgnoreCase(data.name, "step")) return { 0.36f, 0.33f, 0.28f, 1.0f };
			if (ContainsIgnoreCase(data.name, "beam")) return { 0.17f, 0.19f, 0.20f, 1.0f };
			if (ContainsIgnoreCase(data.name, "pillar")) return { 0.48f, 0.37f, 0.22f, 1.0f };
			if (ContainsIgnoreCase(data.name, "cover")) return { 0.39f, 0.31f, 0.24f, 1.0f };
			if (data.collider.collisionType == "Floor") return { 0.29f, 0.27f, 0.24f, 1.0f };
			if (data.collider.collisionType == "Obstacle") return { 0.32f, 0.30f, 0.28f, 1.0f };
			return data.color;
		}
	}

	void StageInstancingManager::Build(
		const LevelData& levelData,
		const std::string& primaryStageModelPath,
		const Vector3& stageOffset)
	{
		Clear();
		std::map<std::string, std::vector<StageInstanceSource>> sourcesByModel;
		auto appendSource = [&](const ObjectData& data)
		{
			if (!IsStageMeshType(data.type) || data.modelName.empty() || (!primaryStageModelPath.empty() && data.modelName == primaryStageModelPath))
			{
				return;
			}

			StageInstanceSource source{};
			source.modelPath = data.modelName;
			source.worldMatrix = Matrix4x4::MakeAffineMatrix(data.scale, data.rotation, data.position + stageOffset);
			source.color = ResolveStageInstanceColor(data);
			sourcesByModel[source.modelPath].push_back(source);
		};

		const std::vector<ObjectData> hiddenArenaObjects = MineHiddenArenaLayout::Build(levelData);
		const std::vector<ObjectData> collapsedCityObjects = CollapsedCityLayout::Build(levelData);
		const std::vector<ObjectData> collapsedCityPassageObjects = CollapsedCityPassageLayout::Build(levelData);
		const std::vector<ObjectData> centralControlObjects = CentralControlColosseumLayout::Build(levelData);
		const bool hasHiddenArena = !hiddenArenaObjects.empty();
		for (const ObjectData& data : levelData.objects)
		{
			if (hasHiddenArena && data.name == "Wall_North") continue; // 旧終端壁だけを外し、中央の封鎖は可動Gateへ置き換える。
			appendSource(data);
		}
		for (const ObjectData& data : hiddenArenaObjects) appendSource(data);
		for (const ObjectData& data : collapsedCityObjects) appendSource(data); // JSONマーカーから生成した都市モジュールを同一モデルの一括描画へ追加する。
		for (const ObjectData& data : collapsedCityPassageObjects) appendSource(data); // 陥没区画の通行橋も都市本体と同じInstance Batchへ統合する。
		for (const ObjectData& data : centralControlObjects) appendSource(data); // 円形Arenaの全石材・機械設備を共通CubeのInstance Batchへまとめる。

		for (auto& [modelPath, sources] : sourcesByModel)
		{
			if (sources.size() < 2)
			{
				uniqueObjects_.push_back(CreateNormalObject(sources.front()));
				continue;
			}

			StageInstanceBatch batch{};
			batch.modelPath = modelPath;
			batch.sources = std::move(sources);
			batch.instances.reserve(batch.sources.size());
			for (const StageInstanceSource& source : batch.sources)
			{
				InstancedObject3DRenderer::InstanceData instance{};
				instance.world = source.worldMatrix;
				instance.worldInverseTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(source.worldMatrix));
				instance.color = source.color;
				batch.instances.push_back(instance);
			}

			batch.renderer = std::make_unique<InstancedObject3DRenderer>();
			batch.renderer->Initialize(batch.modelPath, batch.instances.size());
			batch.renderer->SetFrustumCullingEnabled(true);
			batch.renderer->SetInstances(batch.instances);
			totalInstanceCount_ += batch.instances.size();
			batches_.push_back(std::move(batch));
		}
	}

	void StageInstancingManager::Clear()
	{
		batches_.clear();
		uniqueObjects_.clear();
		totalInstanceCount_ = 0;
	}

	void StageInstancingManager::DrawUniqueObjects()
	{
		for (auto& object : uniqueObjects_)
		{
			if (object) object->Draw();
		}
	}

	void StageInstancingManager::DrawBatchSourcesNormally()
	{
		for (auto& batch : batches_)
		{
			EnsureNormalFallbackObjects(batch);
			for (auto& object : batch.normalFallbackObjects)
			{
				if (object) object->Draw();
			}
		}
	}

	void StageInstancingManager::DrawInstancedBatches()
	{
		for (auto& batch : batches_)
		{
			if (batch.renderer) batch.renderer->Draw();
		}
	}

	void StageInstancingManager::DrawShadow(bool instancingEnabled, bool useInstancedDraw, bool useNormalDraw)
	{
		if (useNormalDraw)
		{
			for (auto& object : uniqueObjects_) if (object) object->DrawShadow();
			if (!instancingEnabled || !useInstancedDraw)
			{
				for (auto& batch : batches_)
				{
					EnsureNormalFallbackObjects(batch);
					for (auto& object : batch.normalFallbackObjects) if (object) object->DrawShadow();
				}
			}
		}
		if (instancingEnabled && useInstancedDraw)
		{
			for (auto& batch : batches_) if (batch.renderer) batch.renderer->DrawShadow();
		}
	}

	void StageInstancingManager::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		for (auto& object : uniqueObjects_) if (object) object->UpdateShadowMatrix(lightViewProjection);
		for (auto& batch : batches_)
		{
			for (auto& object : batch.normalFallbackObjects) if (object) object->UpdateShadowMatrix(lightViewProjection);
		}
	}

	void StageInstancingManager::EnsureNormalFallbackObjects(StageInstanceBatch& batch)
	{
		if (!batch.normalFallbackObjects.empty()) return;
		batch.normalFallbackObjects.reserve(batch.sources.size());
		for (const StageInstanceSource& source : batch.sources)
		{
			batch.normalFallbackObjects.push_back(CreateNormalObject(source));
		}
	}

	std::unique_ptr<Object3D> StageInstancingManager::CreateNormalObject(const StageInstanceSource& source) const
	{
		auto object = std::make_unique<Object3D>();
		object->Initialize(source.modelPath);
		object->UpdateWithWorldMatrix(source.worldMatrix);
		object->SetColor(source.color);
		return object;
	}
} // namespace Ken4lowEngine
