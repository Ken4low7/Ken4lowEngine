#pragma once
#include "ModelData.h"

#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		kmesh 形式のモデルを読み込むクラス
	/// -------------------------------------------------------------
	/// MeshConverter が出力した .kmesh を読み込み、
	/// ランタイム用の ModelData に復元します。
	///
	/// 現段階では static mesh 用の最小実装です。
	/// - 頂点
	/// - インデックス
	/// - サブメッシュ
	/// - テクスチャ参照
	///
	/// までを復元対象とし、
	/// スキニング情報やアニメーション情報はまだ扱いません。
	class KMeshLoader
	{
	public:
		/// <summary>
		/// 論理パスから対応する .kmesh を読み込みます。
		/// 例:
		///   "Characters/body.gltf"
		///    -> "Resources/Models/Compiled/Characters/body.kmesh"
		/// </summary>
		/// <param name="logicalPath">論理パス</param>
		/// <returns>復元された ModelData</returns>
		static ModelData LoadModel(const std::string& logicalPath);

	private:
#pragma pack(push, 1)
		/// <summary>
		/// .kmesh ファイル先頭のヘッダ
		/// MeshConverter 側の FileHeader と一致させる
		/// </summary>
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

		/// <summary>
		/// サブメッシュ単位のヘッダ
		/// MeshConverter 側の BinarySubmeshHeader と一致させる
		/// </summary>
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

		/// <summary>
		/// kmesh に保存されている頂点レイアウト
		/// MeshConverter 側の VertexPNUV と一致させる
		/// </summary>
		struct VertexPNUV
		{
			float px, py, pz;
			float nx, ny, nz;
			float u, v;
		};

	private:
		/// <summary>
		/// magic が kmesh のものか確認する
		/// </summary>
		static bool IsValidMagic(uint32_t magic);

		/// <summary>
		/// VertexPNUV をエンジンの VertexData に変換する
		/// </summary>
		static VertexData ConvertVertex(const VertexPNUV& src);
	};
}