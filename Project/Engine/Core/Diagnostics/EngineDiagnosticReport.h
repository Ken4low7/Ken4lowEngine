#pragma once

#include "BuildProfile.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace Ken4lowEngine
{
	class EngineDiagnosticReport
	{
	public:
		[[nodiscard]] static std::string Build(std::string_view reason = {})
		{
			SYSTEMTIME time{};
			GetLocalTime(&time);

			MEMORYSTATUSEX memory{};
			memory.dwLength = sizeof(memory);
			GlobalMemoryStatusEx(&memory);

			char currentDirectory[MAX_PATH]{};
			const DWORD directoryLength = GetCurrentDirectoryA(MAX_PATH, currentDirectory);

			std::ostringstream report;
			report << "Ken4lowEngine Diagnostic Report\n";
			report << "BuildProfile=" << ToString(GetBuildProfile()) << '\n';
			report << "ProcessId=" << GetCurrentProcessId() << '\n';
			report << "ThreadId=" << GetCurrentThreadId() << '\n';
			report << "Timestamp=" << time.wYear << '-'
				<< time.wMonth << '-' << time.wDay << ' '
				<< time.wHour << ':' << time.wMinute << ':' << time.wSecond << '\n';
			report << "HardwareThreads=" << std::thread::hardware_concurrency() << '\n';
			report << "PhysicalMemoryLoadPercent=" << memory.dwMemoryLoad << '\n';
			report << "AvailablePhysicalMemoryBytes=" << memory.ullAvailPhys << '\n';
			report << "CommandLine=" << GetCommandLineA() << '\n';
			if (directoryLength > 0 && directoryLength < MAX_PATH)
			{
				report << "WorkingDirectory=" << currentDirectory << '\n';
			}
			if (!reason.empty())
			{
				// The reason is caller supplied so crash, soak and manual captures share the same report format.
				report << "Reason=" << reason << '\n';
			}
			return report.str();
		}

		static bool Write(const std::filesystem::path& path, std::string_view reason = {})
		{
			std::error_code error;
			if (path.has_parent_path())
			{
				std::filesystem::create_directories(path.parent_path(), error);
			}
			std::ofstream stream(path, std::ios::trunc);
			if (!stream.is_open())
			{
				return false;
			}
			stream << Build(reason);
			return stream.good();
		}
	};
} // namespace Ken4lowEngine
