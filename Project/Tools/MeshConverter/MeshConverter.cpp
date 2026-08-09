#define NOMINMAX
#include "MeshConverter.h"

#include <iostream>
#include <fstream>
#include <windows.h>
#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <cstdlib>
#include <iomanip>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace std;

// Assimp version compatibility
#ifndef aiTextureType_BASE_COLOR
#define aiTextureType_BASE_COLOR aiTextureType_DIFFUSE
#endif

static constexpr uint32_t MakeMagic(char a, char b, char c, char d)
{
	return (uint32_t(a)) |
		(uint32_t(b) << 8) |
		(uint32_t(c) << 16) |
		(uint32_t(d) << 24);
}

MeshConverter::Options MeshConverter::ParseOptions(int numOptions, char* options[])
{
	Options opt{};
	for (int i = 0; i < numOptions; ++i)
	{
		const std::string s = options[i];

		if (s == "-lh")
		{
			opt.leftHanded = true;
		}
		else if (s == "-rh")
		{
			opt.leftHanded = false;
		}
		else if (s == "-flipuv")
		{
			opt.flipUV = true;
		}
		else if (s == "-no-gen-normal")
		{
			opt.genNormals = false;
		}
		else if (s == "-nonormal")
		{
			// 旧オプション互換
			opt.genNormals = false;
		}
		else if (s == "-notransform")
		{
			opt.preTransform = false;
		}
		else if (s == "-tangent")
		{
			opt.genTangents = true;
		}
		else if (s == "-scale")
		{
			if (i + 1 < numOptions)
			{
				opt.scale = static_cast<float>(atof(options[++i]));
			}
			else
			{
				cerr << "Option -scale requires a value.\n";
			}
		}
	}
	return opt;
}

void MeshConverter::OutputUsage()
{
	std::cout <<
		"MeshConverter.exe <input> [options]\n"
		"Options:\n"
		"  -lh               : export as Left-Handed (flip X + flip winding)\n"
		"  -rh               : export as Right-Handed\n"
		"  -flipuv           : Flip V coordinate\n"
		"  -no-gen-normal    : Do not generate normals\n"
		"  -notransform      : Do not pretransform vertices\n"
		"  -tangent          : Generate tangents (optional)\n"
		"  -scale <f>        : Scale\n";
}

std::wstring MeshConverter::ConvertMultiByteToWide(const std::string& s)
{
	if (s.empty())
	{
		return L"";
	}

	const int required = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	if (required <= 0)
	{
		return L"";
	}

	std::wstring w(static_cast<size_t>(required), L'\0');
	const int written = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), required);
	if (written <= 0)
	{
		return L"";
	}

	if (!w.empty() && w.back() == L'\0')
	{
		w.pop_back();
	}
	return w;
}

void MeshConverter::SeparateFilePath(const std::wstring& filePath)
{
	const size_t pos = filePath.find_last_of(L"\\/");

	if (pos != std::wstring::npos)
	{
		directoryPath_ = filePath.substr(0, pos);
		std::wstring nameExt = filePath.substr(pos + 1);

		const size_t dot = nameExt.find_last_of(L'.');
		if (dot != std::wstring::npos)
		{
			fileName_ = nameExt.substr(0, dot);
			extension_ = nameExt.substr(dot);
		}
		else
		{
			fileName_ = nameExt;
			extension_.clear();
		}
	}
	else
	{
		directoryPath_.clear();

		const size_t dot = filePath.find_last_of(L'.');
		if (dot != std::wstring::npos)
		{
			fileName_ = filePath.substr(0, dot);
			extension_ = filePath.substr(dot);
		}
		else
		{
			fileName_ = filePath;
			extension_.clear();
		}
	}
}

bool MeshConverter::WriteBinary(
	const std::wstring& outPath,
	const std::vector<VertexPNUV>& vertices,
	const std::vector<uint32_t>& indices,
	const std::vector<Submesh>& submeshes,
	FileHeader header)
{
	namespace fs = std::filesystem;

	const fs::path outputPath(outPath);
	if (outputPath.has_parent_path())
	{
		std::error_code ec;
		fs::create_directories(outputPath.parent_path(), ec);
		if (ec)
		{
			std::cerr << "Failed to create output directory: "
				<< outputPath.parent_path().string() << "\n";
			return false;
		}
	}

	header.vertexDataOffset = static_cast<uint32_t>(sizeof(FileHeader));
	header.indexDataOffset =
		header.vertexDataOffset + static_cast<uint32_t>(sizeof(VertexPNUV) * vertices.size());
	header.submeshDataOffset =
		header.indexDataOffset + static_cast<uint32_t>(sizeof(uint32_t) * indices.size());

	std::ofstream ofs(outputPath, std::ios::binary);
	if (!ofs)
	{
		std::cerr << "Failed to open output file: " << outputPath.string() << "\n";
		return false;
	}

	// Header
	ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
	if (!ofs) return false;

	// Vertices
	if (!vertices.empty())
	{
		ofs.write(reinterpret_cast<const char*>(vertices.data()),
			static_cast<std::streamsize>(sizeof(VertexPNUV) * vertices.size()));
		if (!ofs) return false;
	}

	// Indices
	if (!indices.empty())
	{
		ofs.write(reinterpret_cast<const char*>(indices.data()),
			static_cast<std::streamsize>(sizeof(uint32_t) * indices.size()));
		if (!ofs) return false;
	}

	// Submeshes
	for (const auto& sm : submeshes)
	{
		BinarySubmeshHeader smHeader{};
		smHeader.indexOffset = sm.indexOffset;
		smHeader.indexCount = sm.indexCount;
		smHeader.textureRefLength = static_cast<uint32_t>(sm.textureRef.size());
		// Runtime がサブメッシュ頂点範囲を復元できるよう、kmesh に頂点範囲も保存する。
		smHeader.vertexOffset = sm.vertexOffset;
		smHeader.vertexCount = sm.vertexCount;
		smHeader.aabbMin[0] = sm.aabbMin[0];
		smHeader.aabbMin[1] = sm.aabbMin[1];
		smHeader.aabbMin[2] = sm.aabbMin[2];
		smHeader.aabbMax[0] = sm.aabbMax[0];
		smHeader.aabbMax[1] = sm.aabbMax[1];
		smHeader.aabbMax[2] = sm.aabbMax[2];

		ofs.write(reinterpret_cast<const char*>(&smHeader), sizeof(smHeader));
		if (!ofs) return false;

		if (!sm.textureRef.empty())
		{
			ofs.write(sm.textureRef.data(), static_cast<std::streamsize>(sm.textureRef.size()));
			if (!ofs) return false;
		}
	}

	ofs.flush();
	return static_cast<bool>(ofs);
}

bool MeshConverter::ConvertModelToBinary(const std::string& filePath, int numOptions, char* options[])
{
	const Options opt = ParseOptions(numOptions, options);

	// 出力パス決定（入力と同じ場所に .kmesh）
	const std::wstring widePath = ConvertMultiByteToWide(filePath);
	SeparateFilePath(widePath);

	std::wstring outPath;
	if (!directoryPath_.empty())
	{
		outPath = directoryPath_ + L"\\" + fileName_ + L".kmesh";
	}
	else
	{
		outPath = fileName_ + L".kmesh";
	}

	unsigned int flags = 0;
	flags |= aiProcess_Triangulate;
	flags |= aiProcess_JoinIdenticalVertices;
	flags |= aiProcess_SortByPType;

	if (opt.preTransform)
	{
		flags |= aiProcess_PreTransformVertices;
	}

	if (opt.genNormals)
	{
		flags |= aiProcess_GenSmoothNormals;
	}

	if (opt.flipUV)
	{
		flags |= aiProcess_FlipUVs;
	}

	if (opt.genTangents)
	{
		flags |= aiProcess_CalcTangentSpace;
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, flags);
	if (!scene || !scene->HasMeshes())
	{
		cerr << "Assimp ReadFile failed: " << importer.GetErrorString() << endl;
		return false;
	}

	FileHeader header{};
	header.magic = MakeMagic('K', 'M', 'S', 'H');
	header.version = 1;
	header.flags = 0;
	header.vertexDataOffset = 0;
	header.indexDataOffset = 0;
	header.submeshDataOffset = 0;
	header.aabbMin[0] = header.aabbMin[1] = header.aabbMin[2] = +FLT_MAX;
	header.aabbMax[0] = header.aabbMax[1] = header.aabbMax[2] = -FLT_MAX;

	std::vector<VertexPNUV> vertices;
	std::vector<uint32_t> indices;
	std::vector<Submesh> submeshes;

	bool hasNormal = false;
	bool hasUV = false;

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
	{
		const aiMesh* mesh = scene->mMeshes[m];
		if (!mesh)
		{
			cerr << "Null mesh detected.\n";
			return false;
		}

		const bool meshHasUV0 = mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr;
		hasNormal |= mesh->HasNormals();
		hasUV |= meshHasUV0;

#ifdef _DEBUG
		std::cerr << "[MeshConverter][UV] mesh=\"" << mesh->mName.C_Str()
			<< "\" vertices=" << mesh->mNumVertices
			<< " hasUV0=" << (meshHasUV0 ? "true" : "false") << "\n";
#endif

		const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
		const uint32_t baseIndex = static_cast<uint32_t>(indices.size());

		std::string texRef;
		std::string matName;

		if (scene->HasMaterials() && mesh->mMaterialIndex < scene->mNumMaterials)
		{
			const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

			aiString name;
			if (mat && AI_SUCCESS == mat->Get(AI_MATKEY_NAME, name))
			{
				matName = name.C_Str();
			}

			if (mat)
			{
				auto pickTex = [&](aiTextureType type) -> bool
					{
						aiString t;
						if (mat->GetTextureCount(type) > 0 &&
							mat->GetTexture(type, 0, &t) == AI_SUCCESS)
						{
							texRef = t.C_Str();
							return true;
						}
						return false;
					};

				if (!pickTex(aiTextureType_BASE_COLOR))
				{
					pickTex(aiTextureType_DIFFUSE);
				}
			}
		}

		// 同名衝突を避けるため relative path をなるべく維持
		if (!texRef.empty())
		{
			std::filesystem::path p(texRef);
			texRef = p.generic_string();
		}
		else
		{
			texRef = matName;
		}

		Submesh sm{};
		sm.indexOffset = baseIndex;
		sm.vertexOffset = baseVertex;
		sm.vertexCount = mesh->mNumVertices;
		sm.aabbMin[0] = sm.aabbMin[1] = sm.aabbMin[2] = +FLT_MAX;
		sm.aabbMax[0] = sm.aabbMax[1] = sm.aabbMax[2] = -FLT_MAX;

		vertices.reserve(vertices.size() + mesh->mNumVertices);
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			aiVector3D p = mesh->mVertices[i] * opt.scale;

			aiVector3D n(0, 0, 0);
			if (mesh->HasNormals())
			{
				n = mesh->mNormals[i];
			}

			aiVector3D uv(0, 0, 0);
			if (meshHasUV0)
			{
				uv = mesh->mTextureCoords[0][i];
			}

#ifdef _DEBUG
			if (i < 5)
			{
				std::cerr << std::fixed << std::setprecision(6)
					<< "[MeshConverter][UV] mesh=\"" << mesh->mName.C_Str()
					<< "\" vertex=" << i
					<< " uv=(" << uv.x << ", " << uv.y << ")\n";
			}
#endif

			if (opt.leftHanded)
			{
				p.x = -p.x;
				n.x = -n.x;
			}

			VertexPNUV v{};
			v.px = p.x;  v.py = p.y;  v.pz = p.z;
			v.nx = n.x;  v.ny = n.y;  v.nz = n.z;
			v.u = uv.x;  v.v = uv.y;

			header.aabbMin[0] = std::min(header.aabbMin[0], v.px);
			header.aabbMin[1] = std::min(header.aabbMin[1], v.py);
			header.aabbMin[2] = std::min(header.aabbMin[2], v.pz);
			header.aabbMax[0] = std::max(header.aabbMax[0], v.px);
			header.aabbMax[1] = std::max(header.aabbMax[1], v.py);
			header.aabbMax[2] = std::max(header.aabbMax[2], v.pz);

			sm.aabbMin[0] = std::min(sm.aabbMin[0], v.px);
			sm.aabbMin[1] = std::min(sm.aabbMin[1], v.py);
			sm.aabbMin[2] = std::min(sm.aabbMin[2], v.pz);
			sm.aabbMax[0] = std::max(sm.aabbMax[0], v.px);
			sm.aabbMax[1] = std::max(sm.aabbMax[1], v.py);
			sm.aabbMax[2] = std::max(sm.aabbMax[2], v.pz);

			vertices.push_back(v);
		}

		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3)
			{
				continue;
			}

			const uint32_t i0 = baseVertex + face.mIndices[0];
			const uint32_t i1 = baseVertex + face.mIndices[1];
			const uint32_t i2 = baseVertex + face.mIndices[2];

			if (opt.leftHanded)
			{
				indices.push_back(i0);
				indices.push_back(i2);
				indices.push_back(i1);
			}
			else
			{
				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
			}
		}

		sm.indexCount = static_cast<uint32_t>(indices.size()) - baseIndex;
		sm.textureRef = texRef;
		submeshes.push_back(std::move(sm));
	}

	if (vertices.empty())
	{
		cerr << "No vertices were exported.\n";
		return false;
	}

	header.flags = 0;
	if (hasNormal) header.flags |= 1u << 0;
	if (hasUV)     header.flags |= 1u << 1;
	if (opt.genTangents) header.flags |= 1u << 2;

	header.vertexCount = static_cast<uint32_t>(vertices.size());
	header.indexCount = static_cast<uint32_t>(indices.size());
	header.submeshCount = static_cast<uint32_t>(submeshes.size());

	if (!WriteBinary(outPath, vertices, indices, submeshes, header))
	{
		cerr << "Failed to write binary: " << std::filesystem::path(outPath).string() << endl;
		return false;
	}

	wcout << L"Converted: " << outPath << endl;
	return true;
}