#pragma once
#include <LevelData.h>
#include <memory>
#include <string>
#include <json.hpp>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	/// 			　		レベルローダー
	/// -------------------------------------------------------------
	class LevelLoader
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// JSON レベルファイルを読み込み、LevelData を生成して返します。<br/>
		/// ファイルオープンや JSON パースに失敗した場合、assert で異常終了します。
		/// </summary>
		/// <param name="filePath">
		/// 読み込むレベルファイルの相対パス。<br/>
		/// 実際には fileDirectory_ を先頭に付けたパスでアクセスされます。
		/// （例）"Stage1.json" → "Resources/JSON/Stage1.json"
		/// </param>
		/// <returns>
		/// 読み込んだオブジェクト群を保持する LevelData の unique_ptr。
		/// </returns>
		static std::unique_ptr<LevelData> LoadLevel(const std::string& filePath);

	private: /// ---------- 内部ヘルパー ---------- ///

		static void ParseObjectRecursive(
			const nlohmann::json& object,
			LevelData& levelData,
			const Vector3& parentPosition,
			const Vector3& parentRotation,
			const Vector3& parentScale);

	private: /// ---------- メンバ変数 ---------- ///

		/// <summary>
		/// レベルデータ(JSON)が配置されているディレクトリパス。<br/>
		/// LoadLevel() に渡される filePath の先頭に自動で付加されます。
		/// </summary>
		static inline const std::string fileDirectory_ = "Resources/JSON/";
	};


} // namespace Ken4lowEngine
