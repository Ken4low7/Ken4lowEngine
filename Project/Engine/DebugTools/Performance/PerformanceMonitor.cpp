#include "PerformanceMonitor.h"

#define NOMINMAX
#include <Windows.h>
#include <psapi.h>

#include <algorithm>

using namespace Ken4lowEngine;

namespace
{
	unsigned long long ToUInt64(const FILETIME& fileTime)
	{
		ULARGE_INTEGER value{};
		value.LowPart = fileTime.dwLowDateTime;
		value.HighPart = fileTime.dwHighDateTime;
		return value.QuadPart;
	}
}

void PerformanceMonitor::Update(
	float deltaSeconds,
	float fps,
	float updateMs,
	float drawMs,
	float presentMs,
	float sleepMs,
	float totalFrameMs)
{
	stats_.fps = fps;
	stats_.frameTimeMs = (deltaSeconds > 0.0f) ? (deltaSeconds * 1000.0f) : 0.0f;
	stats_.instantFps = (deltaSeconds > 0.0f) ? (1.0f / deltaSeconds) : 0.0f;
	stats_.updateMs = (std::max)(0.0f, updateMs);
	stats_.drawMs = (std::max)(0.0f, drawMs);
	stats_.presentMs = (std::max)(0.0f, presentMs);
	stats_.sleepMs = (std::max)(0.0f, sleepMs);
	stats_.totalFrameMs = totalFrameMs > 0.0f ? totalFrameMs : stats_.frameTimeMs;
	++stats_.frameCount;
	if (stats_.totalFrameMs > spikeThresholdMs_) ++stats_.frameSpikeCount;

	fpsHistory_[historyWriteIndex_] = stats_.instantFps;
	frameTimeHistory_[historyWriteIndex_] = stats_.totalFrameMs;
	updateHistory_[historyWriteIndex_] = stats_.updateMs;
	drawHistory_[historyWriteIndex_] = stats_.drawMs;
	presentHistory_[historyWriteIndex_] = stats_.presentMs;
	sleepHistory_[historyWriteIndex_] = stats_.sleepMs;
	historyWriteIndex_ = (historyWriteIndex_ + 1) % kHistorySize;
	validHistoryCount_ = (std::min)(validHistoryCount_ + 1, kHistorySize);
	RecalculateFrameAggregates(); // 240フレームだけを対象に平均と最大値を毎フレーム更新する。

	statsRefreshAccumulator_ += static_cast<double>(deltaSeconds);
	if (statsRefreshAccumulator_ >= 0.5)
	{
		// CPU使用率は毎フレームではなく一定間隔で更新して計測負荷を抑える。
		UpdateSystemStats();
		statsRefreshAccumulator_ = 0.0;
	}
}

void PerformanceMonitor::Reset()
{
	stats_ = {};
	fpsHistory_.fill(0.0f);
	frameTimeHistory_.fill(0.0f);
	updateHistory_.fill(0.0f);
	drawHistory_.fill(0.0f);
	presentHistory_.fill(0.0f);
	sleepHistory_.fill(0.0f);
	historyWriteIndex_ = 0;
	validHistoryCount_ = 0;
	statsRefreshAccumulator_ = 0.0;
}

void PerformanceMonitor::RecalculateFrameAggregates()
{
	if (validHistoryCount_ == 0)
	{
		stats_.averageFrameTimeMs = 0.0f;
		stats_.maxFrameTimeMs = 0.0f;
		return;
	}

	float total = 0.0f;
	float maximum = 0.0f;
	for (size_t index = 0; index < validHistoryCount_; ++index)
	{
		const float value = frameTimeHistory_[index];
		total += value;
		maximum = (std::max)(maximum, value);
	}
	stats_.averageFrameTimeMs = total / static_cast<float>(validHistoryCount_);
	stats_.maxFrameTimeMs = maximum;
}

void PerformanceMonitor::UpdateSystemStats()
{
	stats_.cpuUsagePercent = ComputeCpuUsagePercent();
	stats_.processCpuUsagePercent = ComputeProcessCpuUsagePercent();

	PROCESS_MEMORY_COUNTERS_EX memoryCounter{};
	if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounter), sizeof(memoryCounter)))
	{
		stats_.memoryUsageMB = static_cast<float>(memoryCounter.WorkingSetSize) / (1024.0f * 1024.0f);
	}
}

float PerformanceMonitor::ComputeCpuUsagePercent()
{
	FILETIME idleTime{}, kernelTime{}, userTime{};
	if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return stats_.cpuUsagePercent;

	const unsigned long long currentIdle = ToUInt64(idleTime);
	const unsigned long long currentKernel = ToUInt64(kernelTime);
	const unsigned long long currentUser = ToUInt64(userTime);

	if (prevSystemKernel_ == 0 && prevSystemUser_ == 0)
	{
		prevSystemIdle_ = currentIdle;
		prevSystemKernel_ = currentKernel;
		prevSystemUser_ = currentUser;
		return stats_.cpuUsagePercent;
	}

	const unsigned long long kernelDelta = currentKernel - prevSystemKernel_;
	const unsigned long long userDelta = currentUser - prevSystemUser_;
	const unsigned long long idleDelta = currentIdle - prevSystemIdle_;
	const unsigned long long total = kernelDelta + userDelta;

	prevSystemIdle_ = currentIdle;
	prevSystemKernel_ = currentKernel;
	prevSystemUser_ = currentUser;
	if (total == 0) return stats_.cpuUsagePercent;

	const double usage = (1.0 - (static_cast<double>(idleDelta) / static_cast<double>(total))) * 100.0;
	return static_cast<float>((usage < 0.0) ? 0.0 : usage);
}

float PerformanceMonitor::ComputeProcessCpuUsagePercent()
{
	FILETIME createTime{}, exitTime{}, kernelTime{}, userTime{};
	if (!GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) return stats_.processCpuUsagePercent;

	const unsigned long long currentKernel = ToUInt64(kernelTime);
	const unsigned long long currentUser = ToUInt64(userTime);

	if (prevProcessKernel_ == 0 && prevProcessUser_ == 0)
	{
		prevProcessKernel_ = currentKernel;
		prevProcessUser_ = currentUser;
		return stats_.processCpuUsagePercent;
	}

	const unsigned long long kernelDelta = currentKernel - prevProcessKernel_;
	const unsigned long long userDelta = currentUser - prevProcessUser_;
	const unsigned long long processDelta = kernelDelta + userDelta;

	prevProcessKernel_ = currentKernel;
	prevProcessUser_ = currentUser;

	SYSTEM_INFO systemInfo{};
	GetSystemInfo(&systemInfo);
	const unsigned int cpuCount = (systemInfo.dwNumberOfProcessors == 0) ? 1 : systemInfo.dwNumberOfProcessors;
	const double interval100ns = 0.5 * 10000000.0;
	const double usage = (static_cast<double>(processDelta) / (interval100ns * static_cast<double>(cpuCount))) * 100.0;
	return static_cast<float>((usage < 0.0) ? 0.0 : usage);
}
