#pragma once

#include "EngineDiagnosticReport.h"

#include <Windows.h>
#include <DbgHelp.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#pragma comment(lib, "Dbghelp.lib")

namespace Ken4lowEngine
{
	class CrashReporter
	{
	public:
		static void Install(std::filesystem::path reportDirectory = "CrashReports")
		{
			std::scoped_lock lock(GetMutex());
			GetReportDirectory() = std::move(reportDirectory);
			std::error_code error;
			std::filesystem::create_directories(GetReportDirectory(), error);
			if (!GetInstalled())
			{
				GetPreviousFilter() = SetUnhandledExceptionFilter(&UnhandledExceptionFilter);
				GetInstalled() = true;
			}
		}

		static void Uninstall()
		{
			std::scoped_lock lock(GetMutex());
			if (!GetInstalled())
			{
				return;
			}
			SetUnhandledExceptionFilter(GetPreviousFilter());
			GetPreviousFilter() = nullptr;
			GetInstalled() = false;
		}

		[[nodiscard]] static std::string CaptureCurrentStackTrace(uint32_t skipFrames = 0)
		{
			constexpr USHORT kMaxFrames = 64;
			void* frames[kMaxFrames]{};
			const USHORT frameCount = CaptureStackBackTrace(
				static_cast<DWORD>(skipFrames + 1), kMaxFrames, frames, nullptr);

			HANDLE process = GetCurrentProcess();
			std::ostringstream output;
			std::scoped_lock lock(GetMutex());
			SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
			const bool symbolsInitialized = SymInitialize(process, nullptr, TRUE) == TRUE;

			for (USHORT index = 0; index < frameCount; ++index)
			{
				const DWORD64 address = reinterpret_cast<DWORD64>(frames[index]);
				output << '#' << index << " 0x" << std::hex << address << std::dec;
				if (symbolsInitialized)
				{
					alignas(SYMBOL_INFO) std::array<std::byte, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> storage{};
					auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
					symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
					symbol->MaxNameLen = MAX_SYM_NAME;
					DWORD64 displacement = 0;
					if (SymFromAddr(process, address, &displacement, symbol))
					{
						output << ' ' << symbol->Name << "+0x" << std::hex << displacement << std::dec;
					}
				}
				output << '\n';
			}

			if (symbolsInitialized)
			{
				SymCleanup(process);
			}
			return output.str();
		}

		[[nodiscard]] static bool WriteManualReport(std::string_view reason)
		{
			const std::filesystem::path basePath = BuildReportBasePath("manual");
			const bool diagnosticsWritten = EngineDiagnosticReport::Write(basePath.string() + ".diagnostic.txt", reason);
			std::ofstream stackStream(basePath.string() + ".stack.txt", std::ios::trunc);
			if (stackStream.is_open())
			{
				// Manual reports use the same symbolization path as unhandled crashes to keep debugging behavior consistent.
				stackStream << CaptureCurrentStackTrace(1);
			}
			return diagnosticsWritten && stackStream.good();
		}

	private:
		static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
		{
			const std::filesystem::path basePath = BuildReportBasePath("crash");
			WriteMiniDump(exceptionPointers, basePath.string() + ".dmp");

			std::ostringstream reason;
			reason << "UnhandledException";
			if (exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr)
			{
				reason << " Code=0x" << std::hex << exceptionPointers->ExceptionRecord->ExceptionCode
					<< " Address=0x" << reinterpret_cast<uintptr_t>(exceptionPointers->ExceptionRecord->ExceptionAddress);
			}
			EngineDiagnosticReport::Write(basePath.string() + ".diagnostic.txt", reason.str());

			std::ofstream stackStream(basePath.string() + ".stack.txt", std::ios::trunc);
			if (stackStream.is_open())
			{
				stackStream << reason.str() << '\n';
				stackStream << CaptureCurrentStackTrace(1);
			}
			return EXCEPTION_EXECUTE_HANDLER;
		}

		static bool WriteMiniDump(EXCEPTION_POINTERS* exceptionPointers, const std::filesystem::path& path)
		{
			const HANDLE file = CreateFileW(
				path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
			exceptionInfo.ThreadId = GetCurrentThreadId();
			exceptionInfo.ExceptionPointers = exceptionPointers;
			exceptionInfo.ClientPointers = FALSE;

			const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
				MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
			const BOOL written = MiniDumpWriteDump(
				GetCurrentProcess(), GetCurrentProcessId(), file, dumpType,
				exceptionPointers != nullptr ? &exceptionInfo : nullptr, nullptr, nullptr);
			CloseHandle(file);
			return written == TRUE;
		}

		[[nodiscard]] static std::filesystem::path BuildReportBasePath(const char* kind)
		{
			SYSTEMTIME time{};
			GetLocalTime(&time);
			char fileName[128]{};
			sprintf_s(fileName, "Ken4low_%s_%04u%02u%02u_%02u%02u%02u_%lu",
				kind,
				static_cast<unsigned>(time.wYear), static_cast<unsigned>(time.wMonth), static_cast<unsigned>(time.wDay),
				static_cast<unsigned>(time.wHour), static_cast<unsigned>(time.wMinute), static_cast<unsigned>(time.wSecond),
				static_cast<unsigned long>(GetCurrentProcessId()));
			return GetReportDirectory() / fileName;
		}

		static std::filesystem::path& GetReportDirectory()
		{
			static std::filesystem::path directory = "CrashReports";
			return directory;
		}

		static LPTOP_LEVEL_EXCEPTION_FILTER& GetPreviousFilter()
		{
			static LPTOP_LEVEL_EXCEPTION_FILTER filter = nullptr;
			return filter;
		}

		static bool& GetInstalled()
		{
			static bool installed = false;
			return installed;
		}

		static std::mutex& GetMutex()
		{
			static std::mutex mutex;
			return mutex;
		}
	};
} // namespace Ken4lowEngine
