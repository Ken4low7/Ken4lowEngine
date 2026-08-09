#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	struct EditorProcessResult
	{
		int exitCode = -1;
		bool launched = false;
	};

	class EditorProcessRunner
	{
	public:
		EditorProcessResult RunBatchFile(
			const std::filesystem::path& batchFile,
			const std::vector<std::string>& arguments,
			const std::filesystem::path& workingDirectory,
			const std::function<void(const std::string&)>& onOutput) const;
	};

} // namespace Ken4lowEngine
