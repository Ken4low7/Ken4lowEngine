#pragma once
#include <string>
#include <vector>
#include <cstdint>

class MeshConverter
{
public:
	MeshConverter() = default;
	~MeshConverter() = default;

	// モデルを読み込んで自前バイナリに変換
	bool ConvertModelToBinary(const std::string& filePath, int numOptions = 0, char* options[] = nullptr);

	// 使い方表示
	static void OutputUsage();

private:
	struct Options
	{
		bool leftHanded = true;      // DX向け（LH）に変換
		bool flipUV = false;         // 必要なら
		bool genNormals = true;      // 法線が無ければ生成
		bool genTangents = false;    // 今回はオプション扱い
		bool preTransform = true;    // ノード変換を焼き込む
		float scale = 1.0f;          // FBX等がcmの時は 0.01 など
	};

#pragma pack(push, 1)
	struct FileHeader
	{
		uint32_t magic;
		uint32_t version;
		uint32_t flags;

		uint32_t vertexCount;
		uint32_t indexCount;
		uint32_t submeshCount;

		uint32_t vertexDataOffset;
		uint32_t indexDataOffset;
		uint32_t submeshDataOffset;

		float aabbMin[3];
		float aabbMax[3];
	};

	struct BinarySubmeshHeader
	{
		uint32_t indexOffset;
		uint32_t indexCount;
		uint32_t textureRefLength;
		uint32_t vertexOffset;
		uint32_t vertexCount;

		float aabbMin[3];
		float aabbMax[3];
	};
#pragma pack(pop)

	struct VertexPNUV
	{
		float px, py, pz;
		float nx, ny, nz;
		float u, v;
	};

	struct Submesh
	{
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
		uint32_t vertexOffset = 0;
		uint32_t vertexCount = 0;
		std::string textureRef;

		float aabbMin[3] = { 0.0f, 0.0f, 0.0f };
		float aabbMax[3] = { 0.0f, 0.0f, 0.0f };
	};

private:
	Options ParseOptions(int numOptions, char* options[]);

	static std::wstring ConvertMultiByteToWide(const std::string& s);
	void SeparateFilePath(const std::wstring& filePathWide);

	bool WriteBinary(
		const std::wstring& outPath,
		const std::vector<VertexPNUV>& vertices,
		const std::vector<uint32_t>& indices,
		const std::vector<Submesh>& submeshes,
		FileHeader header);

private:
	std::wstring directoryPath_;
	std::wstring fileName_;
	std::wstring extension_;
};