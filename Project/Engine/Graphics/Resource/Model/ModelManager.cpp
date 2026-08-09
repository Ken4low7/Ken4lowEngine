#include "ModelManager.h"
#include <AssimpLoader.h>
#include <numeric>
#include <Model.h>

namespace Ken4lowEngine
{

	/// ---------- 初期容量の設定 ----- ///
	constexpr size_t INITIAL_POSITION_CAPACITY = 1000; // 頂点位置（v）の初期容量
	constexpr size_t INITIAL_NORMAL_CAPACITY = 1000;   // 法線（vn）の初期容量
	constexpr size_t INITIAL_TEXCOORD_CAPACITY = 1000; // テクスチャ座標（vt）の初期容量
	constexpr size_t INITIAL_VERTEX_CAPACITY = 3000;   // 頂点データ全体の初期容量


	/// -------------------------------------------------------------
	///					シングルトンインスタンス
	/// -------------------------------------------------------------
	ModelManager* ModelManager::GetInstance()
	{
		static ModelManager instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///					　.objファイルの読み込み
	/// -------------------------------------------------------------
	ModelData ModelManager::LoadObjFile(const std::string& directoryPath, const std::string& filename)
	{
		// 単一OBJ → SubMeshひとつにまとめて返す
		ModelData modelData;
		SubMesh sub;
		std::vector<Vector4> positions; // 頂点位置リスト
		std::vector<Vector3> normals;   // 法線リスト
		std::vector<Vector2> texcoords; // テクスチャリスト

		// ファイルを開く
		std::ifstream file(directoryPath + "/" + filename);
		if (!file.is_open())
		{
			// ファイルが開けなかった場合、例外を投げる
			throw std::runtime_error("Failed to open OBJ file: " + directoryPath + "/" + filename);
		}

		// メモリを事前に確保（処理の効率化）
		positions.reserve(INITIAL_POSITION_CAPACITY);
		normals.reserve(INITIAL_NORMAL_CAPACITY);
		texcoords.reserve(INITIAL_TEXCOORD_CAPACITY);
		sub.vertices.reserve(INITIAL_VERTEX_CAPACITY);

		std::string line;
		while (std::getline(file, line))
		{
			std::istringstream s(line); // 行を文字列ストリームに変換
			std::string identifier;
			s >> identifier;            // 行の識別子を取得

			if (identifier == "v")
			{
				// 頂点位置情報を解析しリストに追加
				positions.push_back(ParseVertex(s));
			}
			else if (identifier == "vt")
			{
				// テクスチャ座標情報を解析しリストに追加
				texcoords.push_back(ParseTexcoord(s));
			}
			else if (identifier == "vn")
			{
				// 法線情報を解析しリストに追加
				normals.push_back(ParseNormal(s));
			}
			else if (identifier == "f")
			{
				// 面情報を解析し、頂点リストに展開して追加
				ParseFace(s, positions, texcoords, normals, sub.vertices);
			}
			else if (identifier == "mtllib")
			{
				// マテリアルテンプレートライブラリファイル名を取得しロード
				std::string materialFilename;
				s >> materialFilename;
				sub.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
			}
		}

		// 非インデックスで頂点を積んだのでインデックスを生成
		sub.indices.resize(sub.vertices.size());
		std::iota(sub.indices.begin(), sub.indices.end(), 0);

		// 解析済みのモデルデータを返す
		modelData.subMeshes.emplace_back(std::move(sub));
		return modelData;
	}


	/// -------------------------------------------------------------
	///					　モデルファイルの読み込み
	/// -------------------------------------------------------------
	std::shared_ptr<Model> ModelManager::LoadModel(const std::string& filePath)
	{
		auto it = models_.find(filePath);
		if (it != models_.end())
		{
			return it->second;
		}

		auto model = std::make_shared<Model>();
		model->Initialize(filePath);
		models_.insert(std::make_pair(filePath, model));
		return model;
	}



	/// -------------------------------------------------------------
	///					　モデルデータの取得関数
	/// -------------------------------------------------------------
	std::shared_ptr<Model> ModelManager::FindModel(const std::string& filePath)
	{
		return LoadModel(filePath);
	}

	void ModelManager::Finalize()
	{
		models_.clear();
	}


	/// -------------------------------------------------------------
	///					　頂点データを解析する
	/// -------------------------------------------------------------
	Vector4 ModelManager::ParseVertex(std::istringstream& s)
	{
		Vector4 position{};
		s >> position.x >> position.y >> position.z;
		position.w = 1.0f;
		return position;
	}


	/// -------------------------------------------------------------
	///				テクスチャ座標データを解析する
	/// -------------------------------------------------------------
	Vector2 ModelManager::ParseTexcoord(std::istringstream& s)
	{
		Vector2 texcoord{};
		s >> texcoord.x >> texcoord.y;
		return texcoord;
	}


	/// -------------------------------------------------------------
	///					法線データを解析する
	/// -------------------------------------------------------------
	Vector3 ModelManager::ParseNormal(std::istringstream& s)
	{
		Vector3 normal{};
		s >> normal.x >> normal.y >> normal.z;
		return normal;
	}


	/// -------------------------------------------------------------
	///		　面情報を解析し、頂点データリストに展開して追加する
	/// -------------------------------------------------------------
	void ModelManager::ParseFace(std::istringstream& s,
		const std::vector<Vector4>& positions,
		const std::vector<Vector2>& texcoords,
		const std::vector<Vector3>& normals,
		std::vector<VertexData>& vertices)
	{
		VertexData triangle[3]; // 三角形の3頂点データ
		for (int32_t i = 0; i < 3; ++i)
		{
			std::string vertexDefinition;
			s >> vertexDefinition; // 面の頂点情報を取得
			std::istringstream v(vertexDefinition);
			uint32_t indices[3] = { 0 }; // 頂点位置 - テクスチャ座標 - 法線のインデックス

			for (int32_t j = 0; j < 3; ++j)
			{
				std::string index;
				if (std::getline(v, index, '/'))
				{
					// " / " で区切られたインデックスを解析（空の場合は0）
					indices[j] = index.empty() ? 0 : std::stoi(index);
				}
			}

			// 各インデックスを使って対応するデータを取得
			Vector4 position = positions[indices[0] - 1];						   // 頂点位置
			Vector2 texcoord = indices[1] ? texcoords[indices[1] - 1] : Vector2{}; // テクスチャ座標
			Vector3 normal = indices[2] ? normals[indices[2] - 1] : Vector3{};	   // 法線

			// データの反転処理（座標系調整）
			position.x *= -1;
			texcoord.y = 1.0f - texcoord.y;
			normal.x *= -1;

			// 頂点データを作成
			triangle[i] = { position, texcoord, normal };
		}

		// 頂点データリストに三角形の頂点を追加（頂点順序を反転）
		vertices.insert(vertices.end(), { triangle[2], triangle[1], triangle[0] });
	}


	/// -------------------------------------------------------------
	///					　.mtlファイルの読み取り
	/// -------------------------------------------------------------
	Material ModelManager::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
	{
		// 1. 必要な変数の宣言
		Material material;
		std::string line;

		// 2. ファイルを開く
		std::ifstream file(directoryPath + "/" + filename);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open material file: " + directoryPath + "/" + filename);
		}

		// 3. 事前準備: ディレクトリパスに"/"を追加
		std::string basePath = directoryPath + "/";

		// 4. ファイルの内容を解析
		while (std::getline(file, line)) {
			// 行の前後の空白を削除（必要に応じて）
			line.erase(0, line.find_first_not_of(" \t"));
			line.erase(line.find_last_not_of(" \t") + 1);

			// 空行やコメント行をスキップ
			if (line.empty() || line[0] == '#') {
				continue;
			}

			std::string identifier;
			std::istringstream s(line);
			s >> identifier;

			// 識別子に基づいて処理を分岐
			if (identifier == "map_Kd") {
				std::string textureFilename;
				if (s >> textureFilename) {
					// テクスチャファイルのパスを構築
					material.textureFilePath = basePath + textureFilename;
				}
				else {
					throw std::runtime_error("Invalid map_Kd format in material file: " + filename);
				}
			}
		}

		// 5. MaterialDataを返す
		return material;
	}

} // namespace Ken4lowEngine
