#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	enum class EditorLogLevel
	{
		Info,
		Warning,
		Error
	};

	/// <summary>Logタブへ表示する時刻・カテゴリ付きの構造化ログです。</summary>
	struct EditorLogEntry
	{
		uint64_t serial = 0;
		EditorLogLevel level = EditorLogLevel::Info;
		std::string timestamp;
		std::string category = "Editor";
		std::string message;
		std::string source;
		uint32_t repeatCount = 1;
	};

	/// <summary>Warning / Errorを同一内容ごとに集約してErrorタブへ表示します。</summary>
	struct EditorDiagnosticIssue
	{
		uint64_t serial = 0;
		EditorLogLevel level = EditorLogLevel::Warning;
		std::string category = "Editor";
		std::string message;
		std::string source;
		std::string firstTimestamp;
		std::string lastTimestamp;
		uint32_t occurrenceCount = 1;
		bool resolved = false;
	};

	struct EditorDiagnosticCounts
	{
		std::size_t infoCount = 0;
		std::size_t warningCount = 0;
		std::size_t errorCount = 0;
		std::size_t activeWarningCount = 0;
		std::size_t activeErrorCount = 0;
	};

	/// <summary>
	/// Editor全体のLogとIssueを共有バッファへ集約します。
	/// 既存コードが個別インスタンスを保持していても同じDiagnostics内容を参照します。
	/// </summary>
	class EditorOutputLog
	{
	public:
		static EditorOutputLog* GetInstance();

		void Add(EditorLogLevel level, const std::string& message);
		void Add(EditorLogLevel level, std::string_view category, const std::string& message, std::string_view source = {});

		void Info(const std::string& message);
		void Warning(const std::string& message);
		void Error(const std::string& message);
		void Info(std::string_view category, const std::string& message, std::string_view source = {});
		void Warning(std::string_view category, const std::string& message, std::string_view source = {});
		void Error(std::string_view category, const std::string& message, std::string_view source = {});

		void Clear();
		void ClearIssues();
		void ClearResolvedIssues();
		void ResolveAllIssues();
		bool SetIssueResolved(uint64_t serial, bool resolved);

		std::vector<EditorLogEntry> GetEntries() const;
		std::vector<EditorDiagnosticIssue> GetIssues() const;
		EditorDiagnosticCounts GetCounts() const;

	private:
		static std::string BuildTimestamp();
		static bool IsSameLog(const EditorLogEntry& entry, EditorLogLevel level, std::string_view category, std::string_view message, std::string_view source);
		static bool IsSameIssue(const EditorDiagnosticIssue& issue, EditorLogLevel level, std::string_view category, std::string_view message, std::string_view source);
		static void TrimBuffers();

		static constexpr std::size_t kMaximumLogEntries = 4096;
		static constexpr std::size_t kMaximumIssues = 512;
		static std::mutex mutex_;
		static std::vector<EditorLogEntry> entries_;
		static std::vector<EditorDiagnosticIssue> issues_;
		static uint64_t nextLogSerial_;
		static uint64_t nextIssueSerial_;
	};

	const char* ToString(EditorLogLevel level);

} // namespace Ken4lowEngine
