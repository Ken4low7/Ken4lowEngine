#pragma once
#include "VertexData.h"
#include "ModelData.h"
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class Model;

	/// -------------------------------------------------------------
	///					モデルマネージャークラス
	/// -------------------------------------------------------------
	class ModelManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ModelManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>ModelManager の唯一のインスタンス。</returns>
		static ModelManager* GetInstance();

		/// <summary>
		/// .obj ファイルを独自パーサーで読み込み、単一 SubMesh として ModelData を生成します。<br/>
		/// ・v / vt / vn / f / mtllib に対応し、<br/>
		/// ・面情報はインデックス展開して非インデックス頂点配列を構築します。<br/>
		/// ロードに失敗した場合は std::runtime_error を送出します。
		/// </summary>
		/// <param name="directoryPath">モデルファイルが存在するディレクトリパス。</param>
		/// <param name="filename">拡張子を含む .obj ファイル名。</param>
		/// <returns>読み込まれたモデルデータ（SubMesh 1 つのみを持つ ModelData）。</returns>
		static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

		/// <summary>
		/// 指定されたファイルパスのモデルを読み込み、Model として管理します。<br/>
		/// 既に同じパスのモデルが読み込まれている場合は何もせず、そのまま保持しているデータを使います。
		/// </summary>
		/// <param name="filePath">モデルファイルのパス。</param>
		std::shared_ptr<Model> LoadModel(const std::string& filePath);

		/// <summary>
		/// 指定されたファイルパスに対応する Model を取得します。<br/>
		/// ・models_ に存在する場合：既存の shared_ptr を返す<br/>
		/// ・存在しない場合：AssimpLoader を使ってモデルをロードし、新しい Model を生成して登録します。<br/>
		/// （Model 側の実装に応じて、必要であれば初期化や ModelData の設定を行います）
		/// </summary>
		/// <param name="filePath">モデルファイルのパス。</param>
		/// <returns>指定パスに対応する Model の shared_ptr。</returns>
		std::shared_ptr<Model> FindModel(const std::string& filePath);

		/// <summary>
		/// 終了処理を実行します。
		/// </summary>
		void Finalize();

	private: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// OBJ の "v" 行をパースし、位置 (x, y, z, w=1.0) を Vector4 として返します。
		/// </summary>
		/// <param name="s">行の残り部分を保持する文字列ストリーム。</param>
		/// <returns>頂点位置ベクトル。</returns>
		static Vector4 ParseVertex(std::istringstream& s);

		/// <summary>
		/// OBJ の "vt" 行をパースし、UV 座標 (x, y) を Vector2 として返します。
		/// </summary>
		/// <param name="s">行の残り部分を保持する文字列ストリーム。</param>
		/// <returns>テクスチャ座標。</returns>
		static Vector2 ParseTexcoord(std::istringstream& s);

		/// <summary>
		/// OBJ の "vn" 行をパースし、法線ベクトル (x, y, z) を Vector3 として返します。
		/// </summary>
		/// <param name="s">行の残り部分を保持する文字列ストリーム。</param>
		/// <returns>法線ベクトル。</returns>
		static Vector3 ParseNormal(std::istringstream& s);

		/// <summary>
		/// OBJ の "f" 行（面情報）をパースし、頂点データリストに三角形として展開して追加します。<br/>
		/// 1 面ごとに 3 つの "頂点位置／テクスチャ座標／法線" インデックスを読み取り、<br/>
		/// 位置・UV・法線を組み合わせた VertexData を 3 つ生成します。<br/>
		/// 座標系の違いを吸収するため、X 反転や UV の V 反転などの調整もここで行います。
		/// </summary>
		/// <param name="s">"f" 行の残り部分。</param>
		/// <param name="positions">事前に読み込まれた頂点位置リスト。</param>
		/// <param name="texcoords">事前に読み込まれたテクスチャ座標リスト。</param>
		/// <param name="normals">事前に読み込まれた法線リスト。</param>
		/// <param name="vertices">展開先の頂点データ配列。</param>
		static void ParseFace(
			std::istringstream& s,
			const std::vector<Vector4>& positions,
			const std::vector<Vector2>& texcoords,
			const std::vector<Vector3>& normals,
			std::vector<VertexData>& vertices);

		/// <summary>
		/// .mtl（マテリアルテンプレートライブラリ）ファイルを読み込み、Material を構築します。<br/>
		/// 現状は主に "map_Kd"（ディフューズテクスチャ）のみを解析し、<br/>
		/// textureFilePath を設定します。<br/>
		/// 読み込みに失敗した場合は std::runtime_error を送出します。
		/// </summary>
		/// <param name="directoryPath">.mtl ファイルが存在するディレクトリパス。</param>
		/// <param name="filename">拡張子を含む .mtl ファイル名。</param>
		/// <returns>読み込まれたマテリアル情報。</returns>
		static Material LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	private: /// ---------- メンバ変数 ---------- ///

		// 読み込んだモデルデータのキャッシュ
		std::unordered_map<std::string, std::shared_ptr<Model>> models_;

		// モデルリソースの格納ディレクトリパス
		const std::string directoryPath = "Resources";

	private: /// ---------- コピー禁止 ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンパターンとして利用します。
		/// </summary>
		ModelManager() = default;

		/// <summary>
		/// デフォルトデストラクタ。
		/// </summary>
		~ModelManager() = default;

		/// <summary>
		/// コピーコンストラクタは使用禁止です。
		/// </summary>
		ModelManager(const ModelManager&) = delete;

		/// <summary>
		/// 代入演算子は使用禁止です。
		/// </summary>
		const ModelManager& operator=(const ModelManager&) = delete;
	};


} // namespace Ken4lowEngine
