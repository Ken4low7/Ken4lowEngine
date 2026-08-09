#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// コンテンツブラウザで扱うアセット種別です。
	/// </summary>
	enum class EditorAssetType
	{
		All,
		Folder,
		Texture,
		Model,
		Animation,
		Material,
		ActorPrefab,
		Level,
		Shader,
		Font,
		Audio,
		Json,
		Other,
	};

	/// <summary>
	/// Asset Registryが保持するファイル・フォルダの軽量情報です。
	/// </summary>
	struct EditorAssetData
	{
		uint64_t id = 0;
		std::string name;
		std::filesystem::path relativePath;
		std::filesystem::path parentPath;
		std::filesystem::path absolutePath;
		std::string extension;
		std::string modifiedTime;
		EditorAssetType type = EditorAssetType::Other;
		uintmax_t sizeBytes = 0;
		bool isDirectory = false;
	};

	/// <summary>
	/// Resources配下を一度走査し、コンテンツブラウザへ統一的なアセット情報を提供します。
	/// </summary>
	class EditorAssetRegistryV2
	{
	public:
		bool Initialize()
		{
			projectDirectory_ = ResolveProjectDirectory();
			contentRoot_ = projectDirectory_ / "Resources";
			return Refresh();
		}

		bool Refresh()
		{
			assets_.clear();
			lastError_.clear();

			std::error_code error;
			if (!std::filesystem::exists(contentRoot_, error) || !std::filesystem::is_directory(contentRoot_, error))
			{
				lastError_ = "Resourcesフォルダが見つかりません: " + contentRoot_.generic_string();
				return false;
			}

			std::filesystem::recursive_directory_iterator iterator(
				contentRoot_,
				std::filesystem::directory_options::skip_permission_denied,
				error);
			const std::filesystem::recursive_directory_iterator end;

			for (; iterator != end; iterator.increment(error))
			{
				if (error)
				{
					lastError_ = "アセット走査中に一部の項目を読み取れませんでした: " + error.message();
					error.clear();
					continue;
				}

				const std::filesystem::directory_entry& entry = *iterator;
				const bool isDirectory = entry.is_directory(error);
				if (error)
				{
					error.clear();
					continue;
				}
				if (!isDirectory && !entry.is_regular_file(error))
				{
					error.clear();
					continue;
				}

				EditorAssetData asset{};
				asset.isDirectory = isDirectory;
				asset.absolutePath = std::filesystem::absolute(entry.path(), error);
				if (error)
				{
					asset.absolutePath = entry.path();
					error.clear();
				}
				asset.relativePath = NormalizeRelative(std::filesystem::relative(entry.path(), contentRoot_, error));
				if (error)
				{
					error.clear();
					continue;
				}
				asset.parentPath = NormalizeRelative(asset.relativePath.parent_path());
				asset.name = entry.path().filename().generic_string();
				asset.extension = isDirectory ? std::string{} : ToLower(entry.path().extension().string());
				asset.type = DetectAssetType(asset.relativePath, asset.extension, isDirectory);
				asset.id = MakeAssetId(asset.relativePath.generic_string());

				if (!isDirectory)
				{
					asset.sizeBytes = entry.file_size(error);
					if (error)
					{
						asset.sizeBytes = 0;
						error.clear();
					}
				}

				const auto writeTime = entry.last_write_time(error);
				if (!error)
				{
					asset.modifiedTime = FormatFileTime(writeTime);
				}
				else
				{
					asset.modifiedTime = "不明";
					error.clear();
				}

				assets_.push_back(std::move(asset));
			}

			std::sort(assets_.begin(), assets_.end(), [](const EditorAssetData& lhs, const EditorAssetData& rhs)
				{
					if (lhs.parentPath != rhs.parentPath)
					{
						return lhs.parentPath.generic_string() < rhs.parentPath.generic_string();
					}
					if (lhs.isDirectory != rhs.isDirectory)
					{
						return lhs.isDirectory;
					}
					return ToLower(lhs.name) < ToLower(rhs.name);
				});
			return true;
		}

		const std::vector<EditorAssetData>& GetAssets() const { return assets_; }
		const std::filesystem::path& GetProjectDirectory() const { return projectDirectory_; }
		const std::filesystem::path& GetContentRoot() const { return contentRoot_; }
		const std::string& GetLastError() const { return lastError_; }

		std::vector<const EditorAssetData*> GetDirectories(const std::filesystem::path& parentPath) const
		{
			const std::filesystem::path normalizedParent = NormalizeRelative(parentPath);
			std::vector<const EditorAssetData*> result;
			for (const EditorAssetData& asset : assets_)
			{
				if (asset.isDirectory && asset.parentPath == normalizedParent)
				{
					result.push_back(&asset);
				}
			}
			return result;
		}

		std::vector<const EditorAssetData*> Query(
			const std::filesystem::path& directory,
			std::string_view searchText,
			EditorAssetType typeFilter) const
		{
			const std::filesystem::path normalizedDirectory = NormalizeRelative(directory);
			const std::string loweredFilter = ToLower(std::string(searchText));
			const bool recursiveSearch = !loweredFilter.empty();
			std::vector<const EditorAssetData*> result;

			for (const EditorAssetData& asset : assets_)
			{
				const bool insideDirectory = recursiveSearch
					? IsSameOrDescendant(asset.parentPath, normalizedDirectory)
					: asset.parentPath == normalizedDirectory;
				if (!insideDirectory)
				{
					continue;
				}

				if (typeFilter != EditorAssetType::All && !asset.isDirectory && asset.type != typeFilter)
				{
					continue;
				}

				if (recursiveSearch)
				{
					const std::string searchable = ToLower(asset.name + " " + asset.relativePath.generic_string() + " " + GetTypeName(asset.type));
					if (searchable.find(loweredFilter) == std::string::npos)
					{
						continue;
					}
				}

				result.push_back(&asset);
			}

			std::sort(result.begin(), result.end(), [](const EditorAssetData* lhs, const EditorAssetData* rhs)
				{
					if (lhs->isDirectory != rhs->isDirectory)
					{
						return lhs->isDirectory;
					}
					return ToLower(lhs->name) < ToLower(rhs->name);
				});
			return result;
		}

		const EditorAssetData* FindById(uint64_t id) const
		{
			const auto found = std::find_if(assets_.begin(), assets_.end(), [id](const EditorAssetData& asset)
				{
					return asset.id == id;
				});
			return found == assets_.end() ? nullptr : &(*found);
		}

		bool IsValidDirectory(const std::filesystem::path& relativePath) const
		{
			const std::filesystem::path normalized = NormalizeRelative(relativePath);
			if (normalized.empty())
			{
				return true;
			}
			return std::any_of(assets_.begin(), assets_.end(), [&normalized](const EditorAssetData& asset)
				{
					return asset.isDirectory && asset.relativePath == normalized;
				});
		}

		std::size_t CountChildren(const std::filesystem::path& relativePath) const
		{
			const std::filesystem::path normalized = NormalizeRelative(relativePath);
			return static_cast<std::size_t>(std::count_if(assets_.begin(), assets_.end(), [&normalized](const EditorAssetData& asset)
				{
					return asset.parentPath == normalized;
				}));
		}

		static const char* GetTypeName(EditorAssetType type)
		{
			switch (type)
			{
			case EditorAssetType::All: return "すべて";
			case EditorAssetType::Folder: return "フォルダ";
			case EditorAssetType::Texture: return "テクスチャ";
			case EditorAssetType::Model: return "モデル";
			case EditorAssetType::Animation: return "アニメーション";
			case EditorAssetType::Material: return "マテリアル";
			case EditorAssetType::ActorPrefab: return "アクタープリファブ";
			case EditorAssetType::Level: return "レベル";
			case EditorAssetType::Shader: return "シェーダー";
			case EditorAssetType::Font: return "フォント";
			case EditorAssetType::Audio: return "オーディオ";
			case EditorAssetType::Json: return "JSON";
			case EditorAssetType::Other: return "その他";
			default: return "不明";
			}
		}

		static const char* GetTypeBadge(EditorAssetType type)
		{
			switch (type)
			{
			case EditorAssetType::Folder: return "DIR";
			case EditorAssetType::Texture: return "TEX";
			case EditorAssetType::Model: return "MESH";
			case EditorAssetType::Animation: return "ANIM";
			case EditorAssetType::Material: return "MAT";
			case EditorAssetType::ActorPrefab: return "ACTOR";
			case EditorAssetType::Level: return "LEVEL";
			case EditorAssetType::Shader: return "HLSL";
			case EditorAssetType::Font: return "FONT";
			case EditorAssetType::Audio: return "AUDIO";
			case EditorAssetType::Json: return "JSON";
			default: return "FILE";
			}
		}

		static std::filesystem::path NormalizeRelative(const std::filesystem::path& path)
		{
			if (path.empty() || path == ".")
			{
				return {};
			}
			const std::filesystem::path normalized = path.lexically_normal();
			if (normalized.empty() || normalized == "." || normalized.generic_string().starts_with(".."))
			{
				return {};
			}
			return normalized;
		}

	private:
		static uint64_t MakeAssetId(std::string_view path)
		{
			uint64_t hash = 14695981039346656037ull;
			for (const char character : path)
			{
				hash ^= static_cast<unsigned char>(character);
				hash *= 1099511628211ull;
			}
			return hash;
		}

		static std::string ToLower(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			return value;
		}

		static bool IsSameOrDescendant(const std::filesystem::path& candidate, const std::filesystem::path& parent)
		{
			const std::string candidateText = NormalizeRelative(candidate).generic_string();
			const std::string parentText = NormalizeRelative(parent).generic_string();
			if (parentText.empty())
			{
				return true;
			}
			return candidateText == parentText || candidateText.starts_with(parentText + "/");
		}

		static EditorAssetType DetectAssetType(
			const std::filesystem::path& relativePath,
			const std::string& extension,
			bool isDirectory)
		{
			if (isDirectory)
			{
				return EditorAssetType::Folder;
			}

			const std::string pathText = ToLower(relativePath.generic_string());
			const std::string fileName = ToLower(relativePath.filename().string());
			if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds" ||
				extension == ".bmp" || extension == ".tga" || extension == ".hdr")
			{
				return EditorAssetType::Texture;
			}
			if (extension == ".gltf" || extension == ".glb" || extension == ".fbx" || extension == ".obj" || extension == ".kmesh")
			{
				if (pathText.find("animation") != std::string::npos || fileName.find("anim") != std::string::npos)
				{
					return EditorAssetType::Animation;
				}
				return EditorAssetType::Model;
			}
			if (extension == ".hlsl" || extension == ".hlsli" || extension == ".cso")
			{
				return EditorAssetType::Shader;
			}
			if (extension == ".ttf" || extension == ".otf" || extension == ".font")
			{
				return EditorAssetType::Font;
			}
			if (extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac")
			{
				return EditorAssetType::Audio;
			}
			if (extension == ".material" || extension == ".mat" || extension == ".mtl" || pathText.find("material") != std::string::npos)
			{
				return EditorAssetType::Material;
			}
			if (extension == ".json")
			{
				if (pathText.find("actorprefab") != std::string::npos || pathText.find("actor_prefab") != std::string::npos)
				{
					return EditorAssetType::ActorPrefab;
				}
				if (pathText.find("level") != std::string::npos || pathText.find("stage") != std::string::npos)
				{
					return EditorAssetType::Level;
				}
				return EditorAssetType::Json;
			}
			return EditorAssetType::Other;
		}

		static std::string FormatFileTime(const std::filesystem::file_time_type& fileTime)
		{
			const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
				fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
			const std::time_t timeValue = std::chrono::system_clock::to_time_t(systemTime);
			std::tm localTime{};
#ifdef _WIN32
			localtime_s(&localTime, &timeValue);
#else
			localtime_r(&timeValue, &localTime);
#endif
			std::ostringstream stream;
			stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
			return stream.str();
		}

		std::filesystem::path ResolveProjectDirectory() const
		{
			std::error_code error;
			std::filesystem::path cursor = std::filesystem::current_path(error);
			for (int depth = 0; depth < 8 && !cursor.empty(); ++depth)
			{
				if (std::filesystem::exists(cursor / "Resources", error))
				{
					return cursor;
				}
				if (std::filesystem::exists(cursor / "Project" / "Resources", error))
				{
					return cursor / "Project";
				}
				cursor = cursor.parent_path();
			}
			return std::filesystem::current_path(error) / "Project";
		}

		std::filesystem::path projectDirectory_;
		std::filesystem::path contentRoot_;
		std::vector<EditorAssetData> assets_;
		std::string lastError_;
	};
} // namespace Ken4lowEngine
