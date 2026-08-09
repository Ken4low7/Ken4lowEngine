#pragma once
#include <string>
#include <filesystem>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		モデルの論理パスを実ファイルパスへ変換するクラス
	/// -------------------------------------------------------------
	/// 例:
	///   論理パス: "Characters/body.gltf"
	///   Sources : "Resources/Models/Sources/Characters/body.gltf"
	///   Compiled: "Resources/Models/Compiled/Characters/body.kmesh"
	///
	/// モデルのロード側では、このクラスを経由して
	/// Sources / Compiled のどちらを見るかを統一します。
	class ModelPathResolver
	{
	public:
		/// <summary>
		/// 論理パスから Sources 側の実パスを作成します。
		/// 例:
		///   "Characters/body.gltf"
		///    -> "Resources/Models/Sources/Characters/body.gltf"
		/// </summary>
		/// <param name="logicalPath">論理パス</param>
		/// <returns>Sources 側の実ファイルパス</returns>
		static std::filesystem::path ToSourcesPath(const std::string& logicalPath);

		/// <summary>
		/// 論理パスから Compiled 側の .kmesh パスを作成します。
		/// 拡張子は必ず .kmesh に置き換えます。
		/// 例:
		///   "Characters/body.gltf"
		///    -> "Resources/Models/Compiled/Characters/body.kmesh"
		/// </summary>
		/// <param name="logicalPath">論理パス</param>
		/// <returns>Compiled 側の .kmesh パス</returns>
		static std::filesystem::path ToCompiledPath(const std::string& logicalPath);

		/// <summary>
		/// 対応する .kmesh が Compiled 側に存在するか確認します。
		/// </summary>
		/// <param name="logicalPath">論理パス</param>
		/// <returns>存在すれば true</returns>
		static bool ExistsCompiled(const std::string& logicalPath);

		/// <summary>
		/// 対応する元モデルが Sources 側に存在するか確認します。
		/// </summary>
		/// <param name="logicalPath">論理パス</param>
		/// <returns>存在すれば true</returns>
		static bool ExistsSource(const std::string& logicalPath);
	};
}