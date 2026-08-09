#include "ModelPathResolver.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		std::filesystem::path NormalizeLogicalModelPath(const std::string& logicalPath)
		{
			std::string normalized = logicalPath;
			std::replace(normalized.begin(), normalized.end(), '\\', '/');

			const std::string prefixes[] = {
				"Resources/Models/Sources/",
				"Models/Sources/",
				"Sources/"
			};
			for (const std::string& prefix : prefixes)
			{
				if (normalized.rfind(prefix, 0) != 0) continue;
				normalized.erase(0, prefix.size());
				break;
			}
			while (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());
			return std::filesystem::path(normalized); // 旧武器JSONのSources接頭辞を論理パスへ戻し、Sources/Sourcesの二重化を防ぐ。
		}
	}

	std::filesystem::path ModelPathResolver::ToSourcesPath(const std::string& logicalPath)
	{
		// Sources 側は元データ置き場
		// 例:
		//   "Characters/body.gltf"
		//    -> "Resources/Models/Sources/Characters/body.gltf"
		return std::filesystem::path("Resources/Models/Sources") / NormalizeLogicalModelPath(logicalPath);
	}

	std::filesystem::path ModelPathResolver::ToCompiledPath(const std::string& logicalPath)
	{
		// Compiled 側は変換後データ置き場
		// 論理パスの拡張子は .kmesh に置き換える
		// 例:
		//   "Characters/body.gltf"
		//    -> "Resources/Models/Compiled/Characters/body.kmesh"
		std::filesystem::path path = std::filesystem::path("Resources/Models/Compiled") / NormalizeLogicalModelPath(logicalPath);
		path.replace_extension(".kmesh");
		return path;
	}

	bool ModelPathResolver::ExistsCompiled(const std::string& logicalPath)
	{
		// Compiled 側に .kmesh があるかを調べる
		return std::filesystem::exists(ToCompiledPath(logicalPath));
	}

	bool ModelPathResolver::ExistsSource(const std::string& logicalPath)
	{
		// 対応する元モデルが Sources 側に存在するかを調べる
		return std::filesystem::exists(ToSourcesPath(logicalPath));
	}
} // namespace Ken4lowEngine
