#include "EditorProcessRunner.h"

#include <array>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		std::string QuoteCommandArgument(const std::string& value)
		{
			std::string quoted = "\"";
			for (char c : value)
			{
				if (c == '"')
				{
					quoted += "\\\"";
				}
				else
				{
					quoted += c;
				}
			}
			quoted += "\"";
			return quoted;
		}

		std::string PathToString(const std::filesystem::path& path)
		{
#ifdef _WIN32
			return path.string();
#else
			return path.generic_string();
#endif
		}
	}

	EditorProcessResult EditorProcessRunner::RunBatchFile(
		const std::filesystem::path& batchFile,
		const std::vector<std::string>& arguments,
		const std::filesystem::path& workingDirectory,
		const std::function<void(const std::string&)>& onOutput) const
	{
		EditorProcessResult result{};
		if (!std::filesystem::exists(batchFile))
		{
			if (onOutput)
			{
				onOutput("Batch file not found: " + batchFile.string());
			}
			return result;
		}

#ifdef _WIN32
		// バッチファイルはcmd経由で実行し、stdout/stderrをOutput Logへ転送する。
		std::ostringstream command;
		command << "cd /d " << QuoteCommandArgument(PathToString(workingDirectory)) << " && ";
		command << QuoteCommandArgument(PathToString(batchFile));
		for (const std::string& argument : arguments)
		{
			command << ' ' << QuoteCommandArgument(argument);
		}
		command << " 2>&1";

		const std::string fullCommand = "cmd /S /C \"" + command.str() + "\"";
		FILE* pipe = _popen(fullCommand.c_str(), "r");
		if (!pipe)
		{
			if (onOutput)
			{
				onOutput("Failed to launch process: " + fullCommand);
			}
			return result;
		}

		result.launched = true;
		std::array<char, 512> buffer{};
		while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
		{
			if (onOutput)
			{
				onOutput(std::string(buffer.data()));
			}
		}
		result.exitCode = _pclose(pipe);
#else
		(void)arguments;
		(void)workingDirectory;
		if (onOutput)
		{
			onOutput("Asset build batch execution is only supported on Windows.");
		}
		result.exitCode = -1;
#endif
		return result;
	}

} // namespace Ken4lowEngine
