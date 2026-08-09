#pragma once

#include "EditorOutputLog.h"

#include <GameTimer.h>
#include <PerformanceMonitor.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// EditorのLog、Profiler、Warning / Errorを一つのDockable Panelへ統合します。
	/// F9で表示を切り替え、旧Output Logの共有バッファも同じ内容として参照します。
	/// </summary>
	class EditorDiagnosticsPanel
	{
	public:
		static EditorDiagnosticsPanel* GetInstance()
		{
			static EditorDiagnosticsPanel instance;
			return &instance;
		}

		void SetVisible(bool visible) { visible_ = visible; }
		bool IsVisible() const { return visible_; }

		void Draw()
		{
#ifdef USE_IMGUI
			UpdateProfiler();

			if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
			{
				visible_ = !visible_; // Diagnosticsを閉じた後もキーボードから再表示できるようにする。
			}

			const EditorDiagnosticCounts counts = outputLog_->GetCounts();
			if (autoOpenOnError_ && counts.activeErrorCount > previousActiveErrorCount_)
			{
				visible_ = true;
				requestErrorTabFocus_ = true;
			}
			previousActiveErrorCount_ = counts.activeErrorCount;
			if (!visible_) return;

			if (ImGui::Begin("診断###Diagnostics", &visible_, ImGuiWindowFlags_MenuBar))
			{
				DrawMenuBar();
				ImGui::TextDisabled("F9: 表示切替");
				ImGui::SameLine();
				DrawStatusSummary(counts);
				ImGui::Separator();

				if (ImGui::BeginTabBar("##DiagnosticsTabs", ImGuiTabBarFlags_Reorderable))
				{
					const std::string logLabel = "ログ (" + std::to_string(counts.infoCount + counts.warningCount + counts.errorCount) + ")###DiagnosticsLog";
					if (ImGui::BeginTabItem(logLabel.c_str()))
					{
						DrawLogTab();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("プロファイラー###DiagnosticsProfiler"))
					{
						DrawProfilerTab();
						ImGui::EndTabItem();
					}

					const std::string errorLabel = "エラー (" + std::to_string(counts.activeErrorCount) + ")###DiagnosticsErrors";
					const ImGuiTabItemFlags errorFlags = requestErrorTabFocus_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
					if (ImGui::BeginTabItem(errorLabel.c_str(), nullptr, errorFlags))
					{
						requestErrorTabFocus_ = false;
						DrawErrorTab();
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();
				}
			}
			ImGui::End();
#endif
		}

	private:
		EditorDiagnosticsPanel() = default;
		~EditorDiagnosticsPanel() = default;
		EditorDiagnosticsPanel(const EditorDiagnosticsPanel&) = delete;
		EditorDiagnosticsPanel& operator=(const EditorDiagnosticsPanel&) = delete;

#ifdef USE_IMGUI
		static ImVec4 GetLevelColor(EditorLogLevel level)
		{
			if (level == EditorLogLevel::Warning) return ImVec4(1.0f, 0.82f, 0.2f, 1.0f);
			if (level == EditorLogLevel::Error) return ImVec4(1.0f, 0.28f, 0.28f, 1.0f);
			return ImGui::GetStyleColorVec4(ImGuiCol_Text);
		}

		static bool ContainsCaseInsensitive(std::string_view text, std::string_view filter)
		{
			if (filter.empty()) return true;
			if (text.find(filter) != std::string_view::npos) return true;

			std::string loweredText(text);
			std::string loweredFilter(filter);
			std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char value)
				{
					return static_cast<char>(std::tolower(value));
				});
			std::transform(loweredFilter.begin(), loweredFilter.end(), loweredFilter.begin(), [](unsigned char value)
				{
					return static_cast<char>(std::tolower(value));
				});
			return loweredText.find(loweredFilter) != std::string::npos;
		}

		static bool MatchesLogSearch(const EditorLogEntry& entry, std::string_view filter)
		{
			return ContainsCaseInsensitive(entry.message, filter) ||
				ContainsCaseInsensitive(entry.category, filter) ||
				ContainsCaseInsensitive(entry.source, filter);
		}

		static bool MatchesIssueSearch(const EditorDiagnosticIssue& issue, std::string_view filter)
		{
			return ContainsCaseInsensitive(issue.message, filter) ||
				ContainsCaseInsensitive(issue.category, filter) ||
				ContainsCaseInsensitive(issue.source, filter);
		}

		void DrawMenuBar()
		{
			if (!ImGui::BeginMenuBar()) return;
			if (ImGui::BeginMenu("表示"))
			{
				ImGui::MenuItem("エラー発生時に開く", nullptr, &autoOpenOnError_);
				ImGui::MenuItem("フレームスパイクをWarningへ記録", nullptr, &reportFrameSpikes_);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("操作"))
			{
				if (ImGui::MenuItem("ログを書き出す")) ExportLogs();
				if (ImGui::MenuItem("ログを消去")) outputLog_->Clear();
				if (ImGui::MenuItem("解決済みIssueを消去")) outputLog_->ClearResolvedIssues();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		void DrawStatusSummary(const EditorDiagnosticCounts& counts)
		{
			ImGui::Text("Info %zu", counts.infoCount);
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(EditorLogLevel::Warning));
			ImGui::Text("Warning %zu / 未解決 %zu", counts.warningCount, counts.activeWarningCount);
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(EditorLogLevel::Error));
			ImGui::Text("Error %zu / 未解決 %zu", counts.errorCount, counts.activeErrorCount);
			ImGui::PopStyleColor();
		}

		void DrawLogTab()
		{
			if (ImGui::Button("消去")) outputLog_->Clear();
			ImGui::SameLine();
			if (ImGui::Button("書き出し")) ExportLogs();
			ImGui::SameLine();
			ImGui::Checkbox("自動スクロール", &logAutoScroll_);
			ImGui::SameLine();
			ImGui::Checkbox("Info", &showInfo_);
			ImGui::SameLine();
			ImGui::Checkbox("Warning", &showWarnings_);
			ImGui::SameLine();
			ImGui::Checkbox("Error", &showErrors_);

			ImGui::SetNextItemWidth((std::max)(220.0f, ImGui::GetContentRegionAvail().x * 0.45f));
			ImGui::InputTextWithHint("##DiagnosticsLogSearch", "メッセージ・カテゴリ・発生元を検索", logSearch_.data(), logSearch_.size());
			if (!lastExportPath_.empty())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("出力: %s", lastExportPath_.c_str());
			}

			const std::vector<EditorLogEntry> entries = outputLog_->GetEntries();
			std::vector<const EditorLogEntry*> visibleEntries;
			visibleEntries.reserve(entries.size());
			for (const EditorLogEntry& entry : entries)
			{
				if (entry.level == EditorLogLevel::Info && !showInfo_) continue;
				if (entry.level == EditorLogLevel::Warning && !showWarnings_) continue;
				if (entry.level == EditorLogLevel::Error && !showErrors_) continue;
				if (!MatchesLogSearch(entry, logSearch_.data())) continue;
				visibleEntries.push_back(&entry);
			}

			const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg |
				ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("##DiagnosticsLogTable", 5, flags, ImVec2(0.0f, 0.0f)))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("時刻", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("レベル", ImGuiTableColumnFlags_WidthFixed, 82.0f);
				ImGui::TableSetupColumn("カテゴリ", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("メッセージ", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("回数", ImGuiTableColumnFlags_WidthFixed, 52.0f);
				ImGui::TableHeadersRow();

				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(visibleEntries.size()));
				while (clipper.Step())
				{
					for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
					{
						const EditorLogEntry& entry = *visibleEntries[static_cast<std::size_t>(row)];
						ImGui::PushID(static_cast<int>(entry.serial));
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(entry.timestamp.c_str());
						ImGui::TableSetColumnIndex(1);
						ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(entry.level));
						ImGui::TextUnformatted(ToString(entry.level));
						ImGui::PopStyleColor();
						ImGui::TableSetColumnIndex(2);
						ImGui::TextUnformatted(entry.category.c_str());
						ImGui::TableSetColumnIndex(3);
						ImGui::TextWrapped("%s%s%s", entry.message.c_str(), entry.source.empty() ? "" : "  [", entry.source.empty() ? "" : entry.source.c_str());
						if (!entry.source.empty())
						{
							ImGui::SameLine(0.0f, 0.0f);
							ImGui::TextUnformatted("]");
						}
						if (ImGui::BeginPopupContextItem("##LogContext"))
						{
							if (ImGui::MenuItem("メッセージをコピー")) ImGui::SetClipboardText(entry.message.c_str());
							ImGui::EndPopup();
						}
						ImGui::TableSetColumnIndex(4);
						ImGui::Text("%u", entry.repeatCount);
						ImGui::PopID();
					}
				}
				if (logAutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 6.0f) ImGui::SetScrollHereY(1.0f);
				ImGui::EndTable();
			}
		}

		void DrawProfilerTab()
		{
			if (ImGui::Button(profilerPaused_ ? "計測再開" : "計測一時停止")) profilerPaused_ = !profilerPaused_;
			ImGui::SameLine();
			if (ImGui::Button("履歴リセット")) profiler_.Reset();
			ImGui::SameLine();
			ImGui::Checkbox("スパイクをWarningへ記録", &reportFrameSpikes_);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.0f);
			if (ImGui::DragFloat("閾値 ms", &frameSpikeThresholdMs_, 0.5f, 1.0f, 500.0f, "%.1f"))
			{
				profiler_.SetSpikeThresholdMs(frameSpikeThresholdMs_);
			}

			const PerformanceStats& stats = profiler_.GetStats();
			const float targetBudgetMs = 1000.0f / static_cast<float>((std::max)(1, GameTimer::GetInstance()->GetTargetFPS()));
			if (ImGui::BeginTable("##ProfilerSummary", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawProfilerMetric("FPS", stats.instantFps, "%.1f");
				DrawProfilerMetric("Frame", stats.totalFrameMs, "%.2f ms");
				DrawProfilerMetric("平均", stats.averageFrameTimeMs, "%.2f ms");
				DrawProfilerMetric("最大", stats.maxFrameTimeMs, "%.2f ms");
				DrawProfilerMetric("Update", stats.updateMs, "%.2f ms");
				DrawProfilerMetric("Draw", stats.drawMs, "%.2f ms");
				DrawProfilerMetric("Present", stats.presentMs, "%.2f ms");
				DrawProfilerMetric("Sleep", stats.sleepMs, "%.2f ms");
				DrawProfilerMetric("CPU", stats.cpuUsagePercent, "%.1f %%");
				DrawProfilerMetric("Process CPU", stats.processCpuUsagePercent, "%.1f %%");
				DrawProfilerMetric("Memory", stats.memoryUsageMB, "%.1f MB");
				DrawProfilerMetric("Spike", static_cast<float>(stats.frameSpikeCount), "%.0f");
				ImGui::EndTable();
			}

			ImGui::TextDisabled("目標フレーム予算: %.2f ms / 完了済み直前フレームを表示", targetBudgetMs);
			const float graphMaximum = (std::max)({ 40.0f, stats.maxFrameTimeMs * 1.15f, frameSpikeThresholdMs_ * 1.15f });
			const int historyOffset = static_cast<int>(profiler_.GetHistoryWriteIndex());
			ImGui::PlotLines("Frame Time", profiler_.GetFrameTimeHistory().data(), static_cast<int>(PerformanceMonitor::kHistorySize), historyOffset, nullptr, 0.0f, graphMaximum, ImVec2(0.0f, 90.0f));
			ImGui::PlotLines("Update", profiler_.GetUpdateHistory().data(), static_cast<int>(PerformanceMonitor::kHistorySize), historyOffset, nullptr, 0.0f, graphMaximum, ImVec2(0.0f, 65.0f));
			ImGui::PlotLines("Draw", profiler_.GetDrawHistory().data(), static_cast<int>(PerformanceMonitor::kHistorySize), historyOffset, nullptr, 0.0f, graphMaximum, ImVec2(0.0f, 65.0f));
			ImGui::PlotLines("Present", profiler_.GetPresentHistory().data(), static_cast<int>(PerformanceMonitor::kHistorySize), historyOffset, nullptr, 0.0f, graphMaximum, ImVec2(0.0f, 65.0f));
		}

		static void DrawProfilerMetric(const char* label, float value, const char* format)
		{
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", label);
			ImGui::Text(format, value);
		}

		void DrawErrorTab()
		{
			if (ImGui::Button("すべて解決")) outputLog_->ResolveAllIssues();
			ImGui::SameLine();
			if (ImGui::Button("解決済みを消去")) outputLog_->ClearResolvedIssues();
			ImGui::SameLine();
			ImGui::Checkbox("未解決", &showActiveIssues_);
			ImGui::SameLine();
			ImGui::Checkbox("解決済み", &showResolvedIssues_);
			ImGui::SameLine();
			ImGui::Checkbox("Warning", &showIssueWarnings_);
			ImGui::SameLine();
			ImGui::Checkbox("Error", &showIssueErrors_);

			ImGui::SetNextItemWidth((std::max)(220.0f, ImGui::GetContentRegionAvail().x * 0.45f));
			ImGui::InputTextWithHint("##DiagnosticsIssueSearch", "Issueを検索", issueSearch_.data(), issueSearch_.size());

			const std::vector<EditorDiagnosticIssue> issues = outputLog_->GetIssues();
			std::vector<const EditorDiagnosticIssue*> visibleIssues;
			visibleIssues.reserve(issues.size());
			for (const EditorDiagnosticIssue& issue : issues)
			{
				if (issue.resolved && !showResolvedIssues_) continue;
				if (!issue.resolved && !showActiveIssues_) continue;
				if (issue.level == EditorLogLevel::Warning && !showIssueWarnings_) continue;
				if (issue.level == EditorLogLevel::Error && !showIssueErrors_) continue;
				if (!MatchesIssueSearch(issue, issueSearch_.data())) continue;
				visibleIssues.push_back(&issue);
			}

			const float detailsHeight = selectedIssueSerial_ != 0 ? 145.0f : 0.0f;
			const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg |
				ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
			if (ImGui::BeginTable("##DiagnosticsIssueTable", 6, flags, ImVec2(0.0f, detailsHeight > 0.0f ? -detailsHeight : 0.0f)))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("状態", ImGuiTableColumnFlags_WidthFixed, 72.0f);
				ImGui::TableSetupColumn("最終", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("レベル", ImGuiTableColumnFlags_WidthFixed, 82.0f);
				ImGui::TableSetupColumn("カテゴリ", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("内容", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("回数", ImGuiTableColumnFlags_WidthFixed, 52.0f);
				ImGui::TableHeadersRow();

				for (const EditorDiagnosticIssue* issue : visibleIssues)
				{
					ImGui::PushID(static_cast<int>(issue->serial));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					const bool selected = selectedIssueSerial_ == issue->serial;
					if (ImGui::Selectable(issue->resolved ? "解決済み" : "未解決", selected, ImGuiSelectableFlags_SpanAllColumns)) selectedIssueSerial_ = issue->serial;
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(issue->lastTimestamp.c_str());
					ImGui::TableSetColumnIndex(2);
					ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(issue->level));
					ImGui::TextUnformatted(ToString(issue->level));
					ImGui::PopStyleColor();
					ImGui::TableSetColumnIndex(3);
					ImGui::TextUnformatted(issue->category.c_str());
					ImGui::TableSetColumnIndex(4);
					ImGui::TextWrapped("%s", issue->message.c_str());
					ImGui::TableSetColumnIndex(5);
					ImGui::Text("%u", issue->occurrenceCount);
					if (ImGui::BeginPopupContextItem("##IssueContext"))
					{
						if (ImGui::MenuItem(issue->resolved ? "再オープン" : "解決にする")) outputLog_->SetIssueResolved(issue->serial, !issue->resolved);
						if (ImGui::MenuItem("内容をコピー")) ImGui::SetClipboardText(issue->message.c_str());
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			const auto selected = std::find_if(issues.begin(), issues.end(), [&](const EditorDiagnosticIssue& issue) { return issue.serial == selectedIssueSerial_; });
			if (selected != issues.end())
			{
				ImGui::SeparatorText("Issue Details");
				ImGui::Text("%s / %s / %u回", ToString(selected->level), selected->category.c_str(), selected->occurrenceCount);
				ImGui::TextDisabled("初回 %s   最終 %s", selected->firstTimestamp.c_str(), selected->lastTimestamp.c_str());
				if (!selected->source.empty()) ImGui::TextDisabled("発生元: %s", selected->source.c_str());
				ImGui::TextWrapped("%s", selected->message.c_str());
				if (ImGui::Button(selected->resolved ? "再オープン" : "解決にする")) outputLog_->SetIssueResolved(selected->serial, !selected->resolved);
				ImGui::SameLine();
				if (ImGui::Button("コピー")) ImGui::SetClipboardText(selected->message.c_str());
			}
		}

		void UpdateProfiler()
		{
			if (profilerPaused_) return;
			GameTimer* timer = GameTimer::GetInstance();
			profiler_.SetSpikeThresholdMs(frameSpikeThresholdMs_);
			profiler_.Update(
				timer->GetDeltaTime(),
				timer->GetFPS(),
				timer->GetUpdateMs(),
				timer->GetDrawMs(),
				timer->GetPresentMs(),
				timer->GetSleepMs(),
				timer->GetTotalFrameMs());

			const PerformanceStats& stats = profiler_.GetStats();
			const double now = ImGui::GetTime();
			if (reportFrameSpikes_ && stats.totalFrameMs > frameSpikeThresholdMs_ && now - lastSpikeReportSeconds_ >= 1.0)
			{
				char message[256]{};
				std::snprintf(
					message,
					sizeof(message),
					"フレームスパイク %.2f ms (Update %.2f / Draw %.2f / Present %.2f / Sleep %.2f)",
					stats.totalFrameMs,
					stats.updateMs,
					stats.drawMs,
					stats.presentMs,
					stats.sleepMs);
				outputLog_->Warning("Profiler", message, "GameTimer");
				lastSpikeReportSeconds_ = now; // 同じ重い状態で毎フレームIssueを増やさないよう通知間隔を制限する。
			}
		}

		static std::string BuildFileTimestamp()
		{
			const auto now = std::chrono::system_clock::now();
			const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
			std::tm localTime{};
#ifdef _WIN32
			localtime_s(&localTime, &timeValue);
#else
			localtime_r(&timeValue, &localTime);
#endif
			std::ostringstream stream;
			stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
			return stream.str();
		}

		void ExportLogs()
		{
			const std::filesystem::path directory = "../Generated/Logs";
			std::error_code error;
			std::filesystem::create_directories(directory, error);
			const std::filesystem::path path = directory / ("EditorDiagnostics_" + BuildFileTimestamp() + ".log");
			std::ofstream file(path);
			if (!file.is_open())
			{
				outputLog_->Error("Diagnostics", "Diagnostics Logを書き出せませんでした。", path.generic_string());
				return;
			}

			for (const EditorLogEntry& entry : outputLog_->GetEntries())
			{
				file << '[' << entry.timestamp << "] [" << ToString(entry.level) << "] [" << entry.category << "] " << entry.message;
				if (!entry.source.empty()) file << " [" << entry.source << ']';
				if (entry.repeatCount > 1) file << " (x" << entry.repeatCount << ')';
				file << '\n';
			}
			file.close();
			lastExportPath_ = path.generic_string();
			outputLog_->Info("Diagnostics", "Diagnostics Logを書き出しました。", lastExportPath_);
		}
#endif

		EditorOutputLog* outputLog_ = EditorOutputLog::GetInstance();
		PerformanceMonitor profiler_{};
		bool visible_ = true;
		bool profilerPaused_ = false;
		bool reportFrameSpikes_ = true;
		bool autoOpenOnError_ = true;
		bool requestErrorTabFocus_ = false;
		bool logAutoScroll_ = true;
		bool showInfo_ = true;
		bool showWarnings_ = true;
		bool showErrors_ = true;
		bool showActiveIssues_ = true;
		bool showResolvedIssues_ = false;
		bool showIssueWarnings_ = true;
		bool showIssueErrors_ = true;
		float frameSpikeThresholdMs_ = 33.333f;
		double lastSpikeReportSeconds_ = -1000.0;
		std::size_t previousActiveErrorCount_ = 0;
		uint64_t selectedIssueSerial_ = 0;
		std::array<char, 256> logSearch_{};
		std::array<char, 256> issueSearch_{};
		std::string lastExportPath_;
	};

} // namespace Ken4lowEngine
