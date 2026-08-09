#include "StageAssetLoader.h"

namespace Ken4lowEngine
{
	namespace
	{
		bool IsStageMeshType(const std::string& type)
		{
			return type == "StaticMesh" || type == "MESH";
		}

		bool IsInstancedOnlyLevel(const LevelData& levelData)
		{
			for (const ObjectData& data : levelData.objects)
			{
				if (data.name.find("InstancedOnlyMarker") != std::string::npos) return true;
			}
			return false; // 専用Markerを持つStageは旧一体型glTFを使わず、生成モジュールだけを描画する。
		}
	}

	std::string StageAssetLoader::ResolveStageModelName(const LevelData& levelData, const std::string& defaultModelName)
	{
		if (IsInstancedOnlyLevel(levelData)) return {};
		// GamePlayから渡された一体型ステージを優先し、個別配置modelPathで誤って置き換えない。
		if (!defaultModelName.empty()) { return defaultModelName; }
		for (const ObjectData& data : levelData.objects)
		{
			if (IsStageMeshType(data.type) && !data.modelName.empty()) { return data.modelName; }
		}
		return {};
	}

	std::unique_ptr<Object3D> StageAssetLoader::BuildStageModel(
		const LevelData& levelData,
		const std::string& defaultModelName,
		const Vector3& offset)
	{
		const std::string modelName = ResolveStageModelName(levelData, defaultModelName);
		if (modelName.empty()) return nullptr;

		auto stageModel = std::make_unique<Object3D>();
		stageModel->Initialize(modelName);
		stageModel->SetTranslate(offset);
		stageModel->SetRotate({ 0.0f, 0.0f, 0.0f });

		Vector3 stageScale{ 1.0f, 1.0f, 1.0f };

		for (const ObjectData& data : levelData.objects)
		{
			if (IsStageMeshType(data.type))
			{
				stageScale = data.scale;
				break;
			}
		}

		stageModel->SetScale(stageScale); // JSON側のステージスケールを描画にも反映する
		stageModel->SetStageObjectCullingUnit(true);

		return stageModel;
	}
} // namespace Ken4lowEngine
