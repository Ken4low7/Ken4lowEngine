#define NOMINMAX
#include "Model.h"
#include "AssimpLoader.h"
#include "TextureManager.h"

#include <algorithm>
#include <limits>
#include <cctype>

namespace Ken4lowEngine
{
	namespace
	{
		bool ShouldUsePointSampling(const std::string& texturePath)
		{
			std::string lowered = texturePath;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lowered.find("face") != std::string::npos ||
				lowered.find("pixel") != std::string::npos ||
				lowered.find("dot") != std::string::npos ||
				lowered.find("nearest") != std::string::npos ||
				lowered.find("nomip") != std::string::npos ||
				lowered.find("pixelart/") != std::string::npos ||
				lowered.find("pixelart\\") != std::string::npos;
		}
	}

	void Model::Initialize(const std::string& filePath)
	{
		modelData_ = AssimpLoader::LoadModel(filePath);

		meshes_.clear();
		materialSRVs_.clear();
		materialUsePointSampling_.clear();

		meshes_.reserve(modelData_.subMeshes.size());
		materialSRVs_.reserve(modelData_.subMeshes.size());
		materialUsePointSampling_.reserve(modelData_.subMeshes.size());

		static const std::string kDefaultTexturePath = "Effects/white.dds";

		BuildLocalBounds();

		for (const auto& sub : modelData_.subMeshes)
		{
			std::string texturePath = sub.material.textureFilePath;
			if (texturePath.empty())
			{
				texturePath = kDefaultTexturePath;
			}

			TextureManager::GetInstance()->LoadTexture(texturePath);
			materialSRVs_.push_back(TextureManager::GetInstance()->GetSrvHandleGPU(texturePath));
			// face/pixel系はデフォルトで Point Sampling を有効化し、まず見た目のぼやけを防ぐ。
			materialUsePointSampling_.push_back(ShouldUsePointSampling(texturePath));

			Mesh mesh{};
			mesh.Initialize(sub.vertices, sub.indices);
			meshes_.push_back(std::move(mesh));
		}
	}

	void Model::BuildLocalBounds()
	{
		meshLocalBounds_.clear();
		meshHasLocalBounds_.clear();
		meshLocalBounds_.reserve(modelData_.subMeshes.size());
		meshHasLocalBounds_.reserve(modelData_.subMeshes.size());

		Vector3 minPos{
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max()
		};
		Vector3 maxPos{
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest()
		};
		bool hasVertex = false;

		for (const auto& sub : modelData_.subMeshes)
		{
			Vector3 meshMin{
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max()
			};
			Vector3 meshMax{
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest()
			};
			bool hasMeshVertex = false;

			for (const auto& vertex : sub.vertices)
			{
				const Vector3 position{ vertex.position.x, vertex.position.y, vertex.position.z };
				minPos.x = std::min(minPos.x, position.x);
				minPos.y = std::min(minPos.y, position.y);
				minPos.z = std::min(minPos.z, position.z);
				maxPos.x = std::max(maxPos.x, position.x);
				maxPos.y = std::max(maxPos.y, position.y);
				maxPos.z = std::max(maxPos.z, position.z);
				meshMin.x = std::min(meshMin.x, position.x);
				meshMin.y = std::min(meshMin.y, position.y);
				meshMin.z = std::min(meshMin.z, position.z);
				meshMax.x = std::max(meshMax.x, position.x);
				meshMax.y = std::max(meshMax.y, position.y);
				meshMax.z = std::max(meshMax.z, position.z);
				hasVertex = true;
				hasMeshVertex = true;
			}

			BoundingSphere meshBounds{};
			if (hasMeshVertex)
			{
				meshBounds.center = (meshMin + meshMax) * 0.5f;
				meshBounds.radius = Vector3::Length(meshMax - meshBounds.center) * 1.1f;
				if (meshBounds.radius <= 0.001f)
				{
					meshBounds.radius = 1.0f;
				}
			}
			meshLocalBounds_.push_back(meshBounds);
			meshHasLocalBounds_.push_back(hasMeshVertex);
		}

		if (!hasVertex)
		{
			localBounds_ = { {}, 1.0f };
			hasLocalBounds_ = false;
			return;
		}

		hasLocalBounds_ = true;
		localBounds_.center = (minPos + maxPos) * 0.5f;
		localBounds_.radius = Vector3::Length(maxPos - localBounds_.center) * 1.1f;
		if (localBounds_.radius <= 0.001f)
		{
			localBounds_.radius = 1.0f;
		}
	}

	uint64_t Model::GetTotalIndexCount() const
	{
		uint64_t total = 0;

		// 高ポリゴンモデルの大量インスタンシング描画を制限するため、全メッシュのインデックス数を合計する。
		for (const auto& mesh : meshes_)
		{
			total += mesh.GetIndexCount();
		}

		return total;
	}
	uint64_t Model::GetTotalVertexCount() const
	{
		uint64_t total = 0;

		// デバッグ表示や描画負荷の見積もりに使うため、全メッシュの頂点数を合計する。
		for (const auto& mesh : meshes_)
		{
			total += mesh.GetVertexCount();
		}

		return total;
	}
}
