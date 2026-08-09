#include "AssimpLoader.h"
#include "TextureManager.h"
#include "ModelPathResolver.h"
#include "LogString.h"

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <algorithm>
#include <format>

namespace Ken4lowEngine
{
	ModelData AssimpLoader::LoadModel(const std::string& modelFilePath)
	{
		// ---------------------------------------------------------
		// 1. 拡張子を取り出して小文字化
		// ---------------------------------------------------------
		// 対応形式チェック用に使う
		std::string extension;
		{
			auto dot = modelFilePath.find_last_of('.');
			if (dot != std::string::npos)
			{
				extension = modelFilePath.substr(dot + 1);
				for (char& c : extension)
				{
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
			}
		}

		// ---------------------------------------------------------
		// 2. 論理パスを Sources 側の実ファイルパスへ変換
		// ---------------------------------------------------------
		// 例:
		//   "Characters/body.gltf"
		//    -> "Resources/Models/Sources/Characters/body.gltf"
		Assimp::Importer importer;
		const std::string filePath = ModelPathResolver::ToSourcesPath(modelFilePath).generic_string();

		// ---------------------------------------------------------
		// 3. Assimp のポストプロセス設定
		// ---------------------------------------------------------
		const unsigned int kFlags =
			aiProcess_Triangulate |          // 三角形化
			aiProcess_JoinIdenticalVertices |// 同一頂点の結合
			aiProcess_GenSmoothNormals |     // 法線生成
			aiProcess_FlipWindingOrder |     // 表裏反転
			aiProcess_FlipUVs;               // UV反転

		const aiScene* scene = nullptr;

		// ---------------------------------------------------------
		// 4. 対応拡張子だけ Assimp で読む
		// ---------------------------------------------------------
		if (extension == "obj" || extension == "gltf" || extension == "glb")
		{
			scene = importer.ReadFile(filePath.c_str(), kFlags);
		}
		else
		{
			throw std::runtime_error("Unsupported file format: " + extension);
		}

		// ---------------------------------------------------------
		// 5. 読み込み結果の検証
		// ---------------------------------------------------------
		if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
		{
			throw std::runtime_error(std::string("Assimp ReadFile failed: ") + importer.GetErrorString());
		}
		if (!scene->HasMeshes())
		{
			throw std::runtime_error("Assimp scene has no meshes: " + filePath);
		}

		// ---------------------------------------------------------
		// 6. ModelData を組み立てる
		// ---------------------------------------------------------
		ModelData modelData;
		ParseMeshes(scene, modelData, modelFilePath);
		modelData.rootNode = ReadNode(scene->mRootNode);

		return modelData;
	}

	Node AssimpLoader::ReadNode(aiNode* node)
	{
		Node result;

		// ---------------------------------------------------------
		// Assimp のノード行列を SRT に分解
		// ---------------------------------------------------------
		aiVector3D scale, translate;
		aiQuaternion rotate;
		node->mTransformation.Decompose(scale, rotate, translate);

		// ---------------------------------------------------------
		// エンジン座標系へ変換
		// ---------------------------------------------------------
		// 既存実装に合わせて
		//   - X を反転
		//   - Quaternion の y,z も反転
		result.transform.scale = { scale.x, scale.y, scale.z };
		result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
		result.transform.translate = { -translate.x, translate.y, translate.z };

		// ローカル行列を再構築
		result.localMatrix = Matrix4x4::MakeAffineMatrix(
			result.transform.scale,
			result.transform.rotate,
			result.transform.translate);

		result.name = node->mName.C_Str();

		// 子ノードを再帰的に読む
		result.children.resize(node->mNumChildren);
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			result.children[i] = ReadNode(node->mChildren[i]);
		}

		return result;
	}

	void AssimpLoader::ParseMeshes(const aiScene* scene, ModelData& modelData, const std::string& modelFilePath)
	{
		// ---------------------------------------------------------
		// baseVertex はスキニングウェイトの頂点番号補正に使う
		// ---------------------------------------------------------
		uint32_t baseVertex = 0;

		for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			aiMesh* mesh = scene->mMeshes[meshIndex];
			if (!mesh)
			{
				continue;
			}

			SubMesh sub;
			sub.vertices.resize(mesh->mNumVertices);

			const bool hasNormals = (mesh->HasNormals() && mesh->mNormals != nullptr);
			const bool hasUV0 = (mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr);

#ifdef _DEBUG
			Log(std::format("[AssimpLoader][UV] model={} mesh=\"{}\" vertices={} hasUV0={}\n",
				modelFilePath, mesh->mName.C_Str(), mesh->mNumVertices, hasUV0 ? "true" : "false"));
#endif

			// -------------------------------------------------
			// 頂点情報の読み出し
			// -------------------------------------------------
			for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
			{
				const aiVector3D& position = mesh->mVertices[vertexIndex];
				sub.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };

				if (hasNormals)
				{
					const aiVector3D& normal = mesh->mNormals[vertexIndex];
					sub.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
				}
				else
				{
					// 法線が無い場合は簡易的に上向きにしておく
					sub.vertices[vertexIndex].normal = { 0.0f, 1.0f, 0.0f };
				}

				if (hasUV0)
				{
					const aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];
					// Assimp の UV0 をそのまま保持し、V反転は明示オプション側だけで行う。
					sub.vertices[vertexIndex].texcoord = { texcoord.x, texcoord.y };
				}
				else
				{
					// UV が無い頂点だけ 0,0 にして、Debugログで UV 欠落を切り分けられるようにする。
					sub.vertices[vertexIndex].texcoord = { 0.0f, 0.0f };
				}

#ifdef _DEBUG
				if (vertexIndex < 5)
				{
					Log(std::format("[AssimpLoader][UV] model={} mesh=\"{}\" vertex={} uv=({:.6f}, {:.6f})\n",
						modelFilePath, mesh->mName.C_Str(), vertexIndex,
						sub.vertices[vertexIndex].texcoord.x, sub.vertices[vertexIndex].texcoord.y));
				}
#endif
			}

			// -------------------------------------------------
			// インデックス情報の読み出し
			// -------------------------------------------------
			for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
			{
				const aiFace& face = mesh->mFaces[faceIndex];
				assert(face.mNumIndices == 3);

				for (uint32_t element = 0; element < face.mNumIndices; ++element)
				{
					const uint32_t vi = face.mIndices[element];
					if (vi < mesh->mNumVertices)
					{
						sub.indices.push_back(vi);
					}
				}
			}

			// -------------------------------------------------
			// ボーン / スキニング情報
			// -------------------------------------------------
			if (mesh->HasBones() && mesh->mBones != nullptr)
			{
				for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
				{
					aiBone* bone = mesh->mBones[boneIndex];
					if (!bone)
					{
						continue;
					}

					const std::string jointName = bone->mName.C_Str();
					JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

					// Assimp のオフセット行列から逆バインドポーズ行列を作る
					const aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();

					aiVector3D scale, translate;
					aiQuaternion rotate;
					bindPoseMatrixAssimp.Decompose(scale, rotate, translate);

					Matrix4x4 bindPoseMatrix = Matrix4x4::MakeAffineMatrix(
						{ scale.x, scale.y, scale.z },
						{ rotate.x, -rotate.y, -rotate.z, rotate.w },
						{ -translate.x, translate.y, translate.z });

					jointWeightData.inverseBindPoseMatrix = Matrix4x4::Inverse(bindPoseMatrix);

					// 各頂点へのウェイトを記録
					if (bone->mWeights != nullptr)
					{
						for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
						{
							const auto& w = bone->mWeights[weightIndex];
							if (w.mVertexId < mesh->mNumVertices)
							{
								jointWeightData.vertexWeights.push_back(
									{ w.mWeight, baseVertex + w.mVertexId });
							}
						}
					}
				}
			}

			// -------------------------------------------------
			// マテリアルからテクスチャ参照を解決
			// -------------------------------------------------
			aiMaterial* material = nullptr;
			if (mesh->mMaterialIndex < scene->mNumMaterials)
			{
				material = scene->mMaterials[mesh->mMaterialIndex];
			}

			if (material && material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				aiString aiTexPath;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS)
				{
					sub.material.textureFilePath = ResolveModelTexturePath(modelFilePath, aiTexPath);
				}
				else
				{
					sub.material.textureFilePath.clear();
				}
			}
			else
			{
				sub.material.textureFilePath.clear();
			}

			modelData.subMeshes.push_back(std::move(sub));
			baseVertex += mesh->mNumVertices;
		}
	}

	std::string AssimpLoader::ResolveModelTexturePath(
		const std::string& modelFilePath,
		const aiString& aiTexPath)
	{
		std::filesystem::path texPath(aiTexPath.C_Str());

		// ---------------------------------------------------------
		// glTF の埋め込みテクスチャ（"*0" のような形式）は今は未対応
		// ---------------------------------------------------------
		const std::string raw = texPath.generic_string();
		if (!raw.empty() && raw[0] == '*')
		{
			return "";
		}

		// 実行時は DDS を使う前提なので拡張子を差し替える
		texPath.replace_extension(".dds");

		// 論理パス基準で、モデルと同じフォルダからの相対位置を作る
		std::filesystem::path modelDir = std::filesystem::path(modelFilePath).parent_path();
		std::filesystem::path relativePath;

		if (texPath.is_absolute())
		{
			// 絶対パスで来た場合はファイル名だけ使う
			relativePath = texPath.filename();
		}
		else
		{
			// モデル相対パスとして解決
			relativePath = (modelDir / texPath).lexically_normal();
		}

		TextureManager* texMgr = TextureManager::GetInstance();

		// 1. モデル相対パスで探す
		std::string found = texMgr->FindCompiledTexturePath(relativePath.generic_string());
		if (!found.empty())
		{
			return found;
		}

		// 2. ファイル名だけで探す
		found = texMgr->FindCompiledTexturePath(texPath.filename().generic_string());
		if (!found.empty())
		{
			return found;
		}

		// 3. 最後の保険
		return "Resources/Textures/Compiled/Debug/uvChecker.dds";
	}

} // namespace Ken4lowEngine