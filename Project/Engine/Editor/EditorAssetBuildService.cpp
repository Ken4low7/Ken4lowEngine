#include "EditorAssetBuildService.h"

#include "EditorOutputLog.h"

#include <chrono>
#include <string>

namespace Ken4lowEngine
{
	namespace
	{
		std::string PathForLog(const std::filesystem::path& path)
		{
			return path.generic_string();
		}
	}

	void EditorAssetBuildService::Initialize(EditorOutputLog* log)
	{
		log_ = log;
		projectDir_ = ResolveProjectDir();
		statusText_ = "Idle";
		if (log_)
		{
			log_->Info("Asset Build Service ready. ProjectDir=" + PathForLog(projectDir_));
		}
	}

	bool EditorAssetBuildService::StartBuild(EditorAssetBuildKind kind)
	{
		Update();
		if (running_.load())
		{
			if (log_)
			{
				log_->Warning("Asset build is already running; ignored request: " + std::string(GetBuildName(kind)));
			}
			return false;
		}

		running_ = true;
		hasLastResult_ = false;
		lastBuildSucceeded_ = false;
		statusText_ = std::string("Running ") + GetBuildName(kind) + "...";
		// Build要求はUIを止めないようバックグラウンドで順番に処理する。
		buildFuture_ = std::async(std::launch::async, [this, kind]()
			{
				return RunBuildSequence(kind);
			});
		return true;
	}

	void EditorAssetBuildService::Update()
	{
		if (!running_.load() || !buildFuture_.valid())
		{
			return;
		}

		if (buildFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			lastBuildSucceeded_ = buildFuture_.get();
			hasLastResult_ = true;
			running_ = false;
			statusText_ = lastBuildSucceeded_ ? "Last build succeeded" : "Last build failed";
		}
	}

	bool EditorAssetBuildService::RunBuildSequence(EditorAssetBuildKind kind)
	{
		bool succeeded = true;
		if (log_)
		{
			log_->Info("Build started: " + std::string(GetBuildName(kind)));
		}

		for (EditorAssetBuildKind singleKind : ExpandBuildKinds(kind))
		{
			if (!RunSingleBuild(singleKind))
			{
				succeeded = false;
				break;
			}
		}

		if (log_)
		{
			if (succeeded)
			{
				log_->Info("Build completed: " + std::string(GetBuildName(kind)));
			}
			else
			{
				log_->Error("Build failed: " + std::string(GetBuildName(kind)));
			}
		}
		return succeeded;
	}

	bool EditorAssetBuildService::RunSingleBuild(EditorAssetBuildKind kind)
	{
		const std::filesystem::path batchFile = GetBatchFile(kind);
		const std::string configuration =
#ifdef _DEBUG
			"Debug";
#else
			"Release";
#endif

		if (log_)
		{
			log_->Info("Running " + std::string(GetBuildName(kind)) + ": " + PathForLog(batchFile));
		}

		const EditorProcessResult result = processRunner_.RunBatchFile(
			batchFile,
			{ projectDir_.string(), configuration },
			projectDir_,
			[this](const std::string& line)
			{
				if (log_)
				{
					std::string message = line;
					while (!message.empty() && (message.back() == '\n' || message.back() == '\r'))
					{
						message.pop_back();
					}
					if (message.empty())
					{
						return;
					}
					// 外部プロセス出力は最低限の分類でOutput Logへ流す。
					if (message.find("error") != std::string::npos || message.find("Error") != std::string::npos || message.find("ERROR") != std::string::npos)
					{
						log_->Error(message);
					}
					else if (message.find("warning") != std::string::npos || message.find("Warning") != std::string::npos || message.find("WARNING") != std::string::npos)
					{
						log_->Warning(message);
					}
					else
					{
						log_->Info(message);
					}
				}
			});

		if (!result.launched)
		{
			if (log_)
			{
				log_->Error("Failed to launch " + std::string(GetBuildName(kind)));
			}
			return false;
		}

		if (result.exitCode != 0)
		{
			if (log_)
			{
				log_->Error(std::string(GetBuildName(kind)) + " exited with code " + std::to_string(result.exitCode));
			}
			return false;
		}

		if (log_)
		{
			log_->Info(std::string(GetBuildName(kind)) + " succeeded.");
		}
		return true;
	}

	std::filesystem::path EditorAssetBuildService::ResolveProjectDir() const
	{
		std::error_code error;
		std::filesystem::path cursor = std::filesystem::current_path(error);
		for (int i = 0; i < 8 && !cursor.empty(); ++i)
		{
			if (std::filesystem::exists(cursor / "Resources", error) && std::filesystem::exists(cursor / "Tools" / "Scripts", error))
			{
				return cursor;
			}
			const std::filesystem::path childProject = cursor / "Project";
			if (std::filesystem::exists(childProject / "Resources", error) && std::filesystem::exists(childProject / "Tools" / "Scripts", error))
			{
				return childProject;
			}
			cursor = cursor.parent_path();
		}
		return std::filesystem::current_path(error) / "Project";
	}

	std::filesystem::path EditorAssetBuildService::GetBatchFile(EditorAssetBuildKind kind) const
	{
		switch (kind)
		{
		case EditorAssetBuildKind::Textures:
			return projectDir_ / "Tools" / "Scripts" / "RunBuildTextures.bat";
		case EditorAssetBuildKind::Meshes:
			return projectDir_ / "Tools" / "Scripts" / "RunBuildMeshes.bat";
		case EditorAssetBuildKind::Fonts:
			return projectDir_ / "Tools" / "Scripts" / "RunBuildFonts.bat";
		default:
			return {};
		}
	}

	std::vector<EditorAssetBuildKind> EditorAssetBuildService::ExpandBuildKinds(EditorAssetBuildKind kind) const
	{
		if (kind == EditorAssetBuildKind::All)
		{
			// Build All Assetsは変換依存を見やすくするためTextures→Meshes→Fontsで固定実行する。
			return { EditorAssetBuildKind::Textures, EditorAssetBuildKind::Meshes, EditorAssetBuildKind::Fonts };
		}
		return { kind };
	}

	const char* EditorAssetBuildService::GetBuildName(EditorAssetBuildKind kind)
	{
		switch (kind)
		{
		case EditorAssetBuildKind::Textures:
			return "Build Textures";
		case EditorAssetBuildKind::Meshes:
			return "Build Meshes";
		case EditorAssetBuildKind::Fonts:
			return "Build Fonts";
		case EditorAssetBuildKind::All:
			return "Build All Assets";
		default:
			return "Build Assets";
		}
	}

} // namespace Ken4lowEngine
