#include "EditorOutputLog.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Ken4lowEngine
{
	std::mutex EditorOutputLog::mutex_{};
	std::vector<EditorLogEntry> EditorOutputLog::entries_{};
	std::vector<EditorDiagnosticIssue> EditorOutputLog::issues_{};
	uint64_t EditorOutputLog::nextLogSerial_ = 1;
	uint64_t EditorOutputLog::nextIssueSerial_ = 1;

	EditorOutputLog* EditorOutputLog::GetInstance()
	{
		static EditorOutputLog instance;
		return &instance;
	}

	std::string EditorOutputLog::BuildTimestamp()
	{
		const auto now = std::chrono::system_clock::now();
		const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
		std::tm localTime{};
#ifdef _WIN32
		localtime_s(&localTime, &timeValue);
#else
		localtime_r(&timeValue, &localTime);
#endif
		std::ostringstream stream;
		stream << std::put_time(&localTime, "%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << milliseconds.count();
		return stream.str();
	}

	bool EditorOutputLog::IsSameLog(const EditorLogEntry& entry, EditorLogLevel level, std::string_view category, std::string_view message, std::string_view source)
	{
		return entry.level == level && entry.category == category && entry.message == message && entry.source == source;
	}

	bool EditorOutputLog::IsSameIssue(const EditorDiagnosticIssue& issue, EditorLogLevel level, std::string_view category, std::string_view message, std::string_view source)
	{
		return !issue.resolved && issue.level == level && issue.category == category && issue.message == message && issue.source == source;
	}

	void EditorOutputLog::TrimBuffers()
	{
		if (entries_.size() > kMaximumLogEntries)
		{
			entries_.erase(entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>(entries_.size() - kMaximumLogEntries));
		}
		if (issues_.size() > kMaximumIssues)
		{
			const auto resolved = std::find_if(issues_.begin(), issues_.end(), [](const EditorDiagnosticIssue& issue) { return issue.resolved; });
			if (resolved != issues_.end()) issues_.erase(resolved);
			else issues_.erase(issues_.begin());
		}
	}

	void EditorOutputLog::Add(EditorLogLevel level, const std::string& message)
	{
		Add(level, "Editor", message, {});
	}

	void EditorOutputLog::Add(EditorLogLevel level, std::string_view category, const std::string& message, std::string_view source)
	{
		if (message.empty()) return;

		const std::string timestamp = BuildTimestamp();
		const std::string normalizedCategory = category.empty() ? "Editor" : std::string(category);
		std::lock_guard<std::mutex> lock(mutex_);

		if (!entries_.empty() && IsSameLog(entries_.back(), level, normalizedCategory, message, source))
		{
			EditorLogEntry& entry = entries_.back();
			entry.timestamp = timestamp;
			++entry.repeatCount; // 連続する同一ログは1行へ集約して大量出力時の可読性を保つ。
		}
		else
		{
			EditorLogEntry entry{};
			entry.serial = nextLogSerial_++;
			entry.level = level;
			entry.timestamp = timestamp;
			entry.category = normalizedCategory;
			entry.message = message;
			entry.source.assign(source.begin(), source.end());
			entries_.push_back(std::move(entry));
		}

		if (level == EditorLogLevel::Warning || level == EditorLogLevel::Error)
		{
			auto found = std::find_if(issues_.begin(), issues_.end(), [&](const EditorDiagnosticIssue& issue)
				{
					return IsSameIssue(issue, level, normalizedCategory, message, source);
				});
			if (found != issues_.end())
			{
				found->lastTimestamp = timestamp;
				++found->occurrenceCount;
			}
			else
			{
				EditorDiagnosticIssue issue{};
				issue.serial = nextIssueSerial_++;
				issue.level = level;
				issue.category = normalizedCategory;
				issue.message = message;
				issue.source.assign(source.begin(), source.end());
				issue.firstTimestamp = timestamp;
				issue.lastTimestamp = timestamp;
				issues_.push_back(std::move(issue));
			}
		}

		TrimBuffers();
	}

	void EditorOutputLog::Info(const std::string& message) { Add(EditorLogLevel::Info, message); }
	void EditorOutputLog::Warning(const std::string& message) { Add(EditorLogLevel::Warning, message); }
	void EditorOutputLog::Error(const std::string& message) { Add(EditorLogLevel::Error, message); }
	void EditorOutputLog::Info(std::string_view category, const std::string& message, std::string_view source) { Add(EditorLogLevel::Info, category, message, source); }
	void EditorOutputLog::Warning(std::string_view category, const std::string& message, std::string_view source) { Add(EditorLogLevel::Warning, category, message, source); }
	void EditorOutputLog::Error(std::string_view category, const std::string& message, std::string_view source) { Add(EditorLogLevel::Error, category, message, source); }

	void EditorOutputLog::Clear()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		entries_.clear();
	}

	void EditorOutputLog::ClearIssues()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		issues_.clear();
	}

	void EditorOutputLog::ClearResolvedIssues()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		std::erase_if(issues_, [](const EditorDiagnosticIssue& issue) { return issue.resolved; });
	}

	void EditorOutputLog::ResolveAllIssues()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (EditorDiagnosticIssue& issue : issues_) issue.resolved = true;
	}

	bool EditorOutputLog::SetIssueResolved(uint64_t serial, bool resolved)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto found = std::find_if(issues_.begin(), issues_.end(), [serial](const EditorDiagnosticIssue& issue) { return issue.serial == serial; });
		if (found == issues_.end()) return false;
		found->resolved = resolved;
		return true;
	}

	std::vector<EditorLogEntry> EditorOutputLog::GetEntries() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return entries_;
	}

	std::vector<EditorDiagnosticIssue> EditorOutputLog::GetIssues() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return issues_;
	}

	EditorDiagnosticCounts EditorOutputLog::GetCounts() const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		EditorDiagnosticCounts counts{};
		for (const EditorLogEntry& entry : entries_)
		{
			const std::size_t amount = static_cast<std::size_t>(entry.repeatCount);
			if (entry.level == EditorLogLevel::Info) counts.infoCount += amount;
			else if (entry.level == EditorLogLevel::Warning) counts.warningCount += amount;
			else if (entry.level == EditorLogLevel::Error) counts.errorCount += amount;
		}
		for (const EditorDiagnosticIssue& issue : issues_)
		{
			if (issue.resolved) continue;
			if (issue.level == EditorLogLevel::Warning) ++counts.activeWarningCount;
			else if (issue.level == EditorLogLevel::Error) ++counts.activeErrorCount;
		}
		return counts;
	}

	const char* ToString(EditorLogLevel level)
	{
		switch (level)
		{
		case EditorLogLevel::Info: return "Info";
		case EditorLogLevel::Warning: return "Warning";
		case EditorLogLevel::Error: return "Error";
		default: return "Info";
		}
	}

} // namespace Ken4lowEngine
