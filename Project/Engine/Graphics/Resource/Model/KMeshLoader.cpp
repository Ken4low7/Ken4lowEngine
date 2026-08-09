#include "KMeshLoader.h"
#include "ModelPathResolver.h"

#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <format>
#include "LogString.h"

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// "KMSH" を uint32_t にした値
		/// MeshConverter 側の MakeMagic('K','M','S','H') と揃える
		/// </summary>
		constexpr uint32_t MakeMagic(char a, char b, char c, char d)
		{
			return (uint32_t(a)) |
				(uint32_t(b) << 8) |
				(uint32_t(c) << 16) |
				(uint32_t(d) << 24);
		}

		constexpr uint32_t kKMeshMagic = MakeMagic('K', 'M', 'S', 'H');
		constexpr uint32_t kSupportedVersion = 1;
	}

	bool KMeshLoader::IsValidMagic(uint32_t magic)
	{
		return magic == kKMeshMagic;
	}

	VertexData KMeshLoader::ConvertVertex(const VertexPNUV& src)
	{
		VertexData dst{};
		dst.position = { src.px, src.py, src.pz, 1.0f };
		dst.texcoord = { src.u, src.v };
		dst.normal = { src.nx, src.ny, src.nz };
		return dst;
	}

	ModelData KMeshLoader::LoadModel(const std::string& logicalPath)
	{
		// ---------------------------------------------------------
		// 1. 読み込む .kmesh の実パスを解決
		// ---------------------------------------------------------
		const std::filesystem::path filePath = ModelPathResolver::ToCompiledPath(logicalPath);

		if (!std::filesystem::exists(filePath))
		{
			throw std::runtime_error("KMesh file not found: " + filePath.generic_string());
		}

		// ---------------------------------------------------------
		// 2. ファイルを開く
		// ---------------------------------------------------------
		std::ifstream ifs(filePath, std::ios::binary);
		if (!ifs)
		{
			throw std::runtime_error("Failed to open KMesh file: " + filePath.generic_string());
		}

		// ---------------------------------------------------------
		// 3. ヘッダ読込
		// ---------------------------------------------------------
		FileHeader header{};
		ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (!ifs)
		{
			throw std::runtime_error("Failed to read KMesh header: " + filePath.generic_string());
		}

		// magic チェック
		if (!IsValidMagic(header.magic))
		{
			throw std::runtime_error("Invalid KMesh magic: " + filePath.generic_string());
		}

		// version チェック
		if (header.version != kSupportedVersion)
		{
			throw std::runtime_error("Unsupported KMesh version: " + filePath.generic_string());
		}

		// ---------------------------------------------------------
		// 4. 頂点配列読込
		// ---------------------------------------------------------
		std::vector<VertexPNUV> fileVertices(header.vertexCount);

		ifs.seekg(static_cast<std::streamoff>(header.vertexDataOffset), std::ios::beg);
		if (!ifs)
		{
			throw std::runtime_error("Failed to seek vertex data: " + filePath.generic_string());
		}

		if (!fileVertices.empty())
		{
			ifs.read(reinterpret_cast<char*>(fileVertices.data()),
				static_cast<std::streamsize>(sizeof(VertexPNUV) * fileVertices.size()));
			if (!ifs)
			{
				throw std::runtime_error("Failed to read vertex data: " + filePath.generic_string());
			}
		}

#ifdef _DEBUG
		const bool hasUV0 = (header.flags & (1u << 1)) != 0;
		Log(std::format("[KMeshLoader][UV] file={} vertices={} hasUV0={}\n",
			filePath.generic_string(), header.vertexCount, hasUV0 ? "true" : "false"));
		for (size_t i = 0; i < std::min<size_t>(fileVertices.size(), 5); ++i)
		{
			Log(std::format("[KMeshLoader][UV] file={} vertex={} uv=({:.6f}, {:.6f})\n",
				filePath.generic_string(), i, fileVertices[i].u, fileVertices[i].v));
		}
#endif

		// ---------------------------------------------------------
		// 5. インデックス配列読込
		// ---------------------------------------------------------
		std::vector<uint32_t> fileIndices(header.indexCount);

		ifs.seekg(static_cast<std::streamoff>(header.indexDataOffset), std::ios::beg);
		if (!ifs)
		{
			throw std::runtime_error("Failed to seek index data: " + filePath.generic_string());
		}

		if (!fileIndices.empty())
		{
			ifs.read(reinterpret_cast<char*>(fileIndices.data()),
				static_cast<std::streamsize>(sizeof(uint32_t) * fileIndices.size()));
			if (!ifs)
			{
				throw std::runtime_error("Failed to read index data: " + filePath.generic_string());
			}
		}

		// ---------------------------------------------------------
		// 6. ModelData を組み立てる
		// ---------------------------------------------------------
		ModelData modelData;

		// 現段階では static mesh 前提なので、
		// skinClusterData は空のまま、
		// rootNode も最小限にしておく
		modelData.rootNode.name = "KMeshRoot";

		// 必要ならここで localMatrix を単位行列にする
		// あなたの Matrix4x4 側の関数名に合わせて調整してください
		// 例:
		// modelData.rootNode.localMatrix = Matrix4x4::MakeIdentity4x4();

		// ---------------------------------------------------------
		// 7. サブメッシュ情報読込
		// ---------------------------------------------------------
		ifs.seekg(static_cast<std::streamoff>(header.submeshDataOffset), std::ios::beg);
		if (!ifs)
		{
			throw std::runtime_error("Failed to seek submesh data: " + filePath.generic_string());
		}

		for (uint32_t smIndex = 0; smIndex < header.submeshCount; ++smIndex)
		{
			BinarySubmeshHeader smHeader{};
			ifs.read(reinterpret_cast<char*>(&smHeader), sizeof(smHeader));
			if (!ifs)
			{
				throw std::runtime_error("Failed to read submesh header: " + filePath.generic_string());
			}

			// textureRef 文字列を読む
			std::string textureRef;
			if (smHeader.textureRefLength > 0)
			{
				textureRef.resize(smHeader.textureRefLength);
				ifs.read(textureRef.data(), static_cast<std::streamsize>(smHeader.textureRefLength));
				if (!ifs)
				{
					throw std::runtime_error("Failed to read submesh textureRef: " + filePath.generic_string());
				}
			}

			SubMesh subMesh{};

			const bool hasStoredVertexRange = smHeader.vertexCount != 0;
			const uint32_t vertexBegin = hasStoredVertexRange ? smHeader.vertexOffset : 0;
			const uint32_t vertexCount = hasStoredVertexRange ? smHeader.vertexCount : static_cast<uint32_t>(fileVertices.size());
			const uint32_t vertexEnd = vertexBegin + vertexCount;
			if (vertexEnd > fileVertices.size())
			{
				throw std::runtime_error("Submesh vertex range out of bounds: " + filePath.generic_string());
			}

			// kmesh に保存されたサブメッシュ頂点範囲だけを読み戻し、UVを含む頂点レイアウトを維持する。
			subMesh.vertices.resize(vertexCount);
			for (uint32_t i = 0; i < vertexCount; ++i)
			{
				subMesh.vertices[i] = ConvertVertex(fileVertices[vertexBegin + i]);
			}

#ifdef _DEBUG
			Log(std::format("[KMeshLoader][UV] submesh={} texture=\"{}\" vertexOffset={} vertexCount={} indexOffset={} indexCount={} rangeFallback={}\n",
				smIndex, textureRef, vertexBegin, vertexCount, smHeader.indexOffset, smHeader.indexCount, hasStoredVertexRange ? "false" : "true"));
#endif

			// -------------------------------------------------
			// インデックスはこのサブメッシュ範囲だけ抜き出す
			// -------------------------------------------------
			const uint32_t beginIndex = smHeader.indexOffset;
			const uint32_t endIndex = smHeader.indexOffset + smHeader.indexCount;

			if (endIndex > fileIndices.size())
			{
				throw std::runtime_error("Submesh index range out of bounds: " + filePath.generic_string());
			}

			subMesh.indices.reserve(smHeader.indexCount);
			for (uint32_t i = beginIndex; i < endIndex; ++i)
			{
				if (fileIndices[i] < vertexBegin || fileIndices[i] >= vertexEnd)
				{
					throw std::runtime_error("Submesh index references a vertex outside its range: " + filePath.generic_string());
				}
				// サブメッシュ内のローカル頂点配列に合わせてインデックスを再基準化する。
				subMesh.indices.push_back(fileIndices[i] - vertexBegin);
			}

			// マテリアルのテクスチャ参照
			subMesh.material.textureFilePath = textureRef;

			modelData.subMeshes.push_back(std::move(subMesh));
		}

		return modelData;
	}
}