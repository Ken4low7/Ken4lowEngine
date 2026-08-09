#pragma once

#include "EditorProcessRunner.h"

#include <atomic>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class EditorOutputLog;

	enum class EditorAssetBuildKind
	{
		Textures,
		Meshes,
		Fonts,
		All
	};

	class EditorAssetBuildService
	{
	public:
		void Initialize(EditorOutputLog* log);
		bool StartBuild(EditorAssetBuildKind kind);
		void Update();
		bool IsRunning() const { return running_.load(); }
		bool HasLastResult() const { return hasLastResult_; }
		bool WasLastBuildSuccessful() const { return lastBuildSucceeded_; }
		const std::string& GetStatusText() const { return statusText_; }

	private:
		bool RunBuildSequence(EditorAssetBuildKind kind);
		bool RunSingleBuild(EditorAssetBuildKind kind);
		std::filesystem::path ResolveProjectDir() const;
		std::filesystem::path GetBatchFile(EditorAssetBuildKind kind) const;
		std::vector<EditorAssetBuildKind> ExpandBuildKinds(EditorAssetBuildKind kind) const;
		static const char* GetBuildName(EditorAssetBuildKind kind);

		EditorOutputLog* log_ = nullptr;
		EditorProcessRunner processRunner_;
		std::filesystem::path projectDir_;
		std::future<bool> buildFuture_;
		std::atomic_bool running_ = false;
		bool hasLastResult_ = false;
		bool lastBuildSucceeded_ = false;
		std::string statusText_ = "Idle";
	};

} // namespace Ken4lowEngine
