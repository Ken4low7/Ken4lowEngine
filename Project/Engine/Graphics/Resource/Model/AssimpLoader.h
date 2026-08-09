#pragma once
#include "ModelData.h"

#include <string>
#include <filesystem>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	/// 				Assimpでモデルを読み込むクラス
	/// -------------------------------------------------------------
	class AssimpLoader
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// Assimp を使ってモデルファイルを読み込み、ModelData を生成します。<br/>
		/// 対応形式は .obj / .gltf / .glb で、ファイル拡張子によって処理を分岐します。<br/>
		/// 三角形化・頂点のマージ・法線生成・表裏反転・UV 反転などのポストプロセスを行った上で、<br/>
		/// SubMesh / Node / SkinClusterData を組み立てます。
		/// </summary>
		/// <param name="modelFilePath">
		/// モデルファイルのパス。<br/>
		/// 内部では "Resources/Models/" を先頭に付けて読み込みます。
		/// 例) "Player/Player.gltf"
		/// </param>
		/// <returns>読み込まれたモデルデータ</returns>
		static ModelData LoadModel(const std::string& modelFilePath);

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// Assimp の aiNode から自前の Node 構造体を再帰的に構築します。<br/>
		/// ・aiNode::mTransformation から SRT を分解し、Transform に詰め直す<br/>
		/// ・座標系の違いを吸収するために X 軸反転や回転方向の反転を行う<br/>
		/// ・子ノードを再帰的に辿って children に格納する<br/>
		/// といった処理を行います。
		/// </summary>
		/// <param name="node">Assimp のノードポインタ。</param>
		/// <returns>変換された Node 構造体。</returns>
		static Node ReadNode(aiNode* node);

		/// <summary>
		/// Assimp のシーンからメッシュ情報を解析し、ModelData に SubMesh と SkinClusterData を構築します。<br/>
		/// ・各 aiMesh から頂点配列（位置 / 法線 / UV）とインデックス配列を生成<br/>
		/// ・X 軸反転などの座標系変換を適用<br/>
		/// ・aiBone からジョイントごとのウェイトと逆バインドポーズ行列を計算して SkinClusterData に格納<br/>
		/// ・aiMaterial から Diffuse テクスチャ (aiTextureType_DIFFUSE) のファイル名を取得し、textureFilePath に設定<br/>
		/// といった処理を行います。
		/// </summary>
		/// <param name="scene">Assimp が解析したシーンデータ</param>
		/// <param name="modelData">解析結果を書き込む ModelData</param>
		static void ParseMeshes(const aiScene* scene, ModelData& modelData, const std::string& modelFilePath);

		static std::string ResolveModelTexturePath(const std::string& modelFilePath, const aiString& aiTexPath);
	};


} // namespace Ken4lowEngine
