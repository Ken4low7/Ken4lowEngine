#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Ken4lowEngine
{
	struct PerformanceStats
	{
		float fps = 0.0f; // 1秒単位で集計した平均FPS
		float instantFps = 0.0f; // frameTimeMs と同じ deltaSeconds から算出した瞬間FPS
		float frameTimeMs = 0.0f;
		float updateMs = 0.0f;
		float drawMs = 0.0f;
		float presentMs = 0.0f;
		float sleepMs = 0.0f;
		float totalFrameMs = 0.0f;
		float averageFrameTimeMs = 0.0f;
		float maxFrameTimeMs = 0.0f;
		float cpuUsagePercent = 0.0f;
		float processCpuUsagePercent = 0.0f;
		float memoryUsageMB = 0.0f;
		uint64_t frameCount = 0;
		uint64_t frameSpikeCount = 0;
	};

	class PerformanceMonitor
	{
	public:
		static constexpr size_t kHistorySize = 240;

		void Update(
			float deltaSeconds,
			float fps,
			float updateMs = 0.0f,
			float drawMs = 0.0f,
			float presentMs = 0.0f,
			float sleepMs = 0.0f,
			float totalFrameMs = 0.0f);
		void Reset();
		void SetSpikeThresholdMs(float thresholdMs) { spikeThresholdMs_ = thresholdMs > 0.0f ? thresholdMs : 1.0f; }

		const PerformanceStats& GetStats() const { return stats_; }
		const std::array<float, kHistorySize>& GetFpsHistory() const { return fpsHistory_; }
		const std::array<float, kHistorySize>& GetFrameTimeHistory() const { return frameTimeHistory_; }
		const std::array<float, kHistorySize>& GetUpdateHistory() const { return updateHistory_; }
		const std::array<float, kHistorySize>& GetDrawHistory() const { return drawHistory_; }
		const std::array<float, kHistorySize>& GetPresentHistory() const { return presentHistory_; }
		const std::array<float, kHistorySize>& GetSleepHistory() const { return sleepHistory_; }
		size_t GetHistoryWriteIndex() const { return historyWriteIndex_; }
		size_t GetValidHistoryCount() const { return validHistoryCount_; }
		float GetSpikeThresholdMs() const { return spikeThresholdMs_; }

	private:
		void UpdateSystemStats();
		void RecalculateFrameAggregates();
		float ComputeCpuUsagePercent();
		float ComputeProcessCpuUsagePercent();

	private:
		PerformanceStats stats_{};
		std::array<float, kHistorySize> fpsHistory_{};
		std::array<float, kHistorySize> frameTimeHistory_{};
		std::array<float, kHistorySize> updateHistory_{};
		std::array<float, kHistorySize> drawHistory_{};
		std::array<float, kHistorySize> presentHistory_{};
		std::array<float, kHistorySize> sleepHistory_{};
		size_t historyWriteIndex_ = 0;
		size_t validHistoryCount_ = 0;
		float spikeThresholdMs_ = 33.333f;

		double statsRefreshAccumulator_ = 0.0;

		unsigned long long prevSystemIdle_ = 0;
		unsigned long long prevSystemKernel_ = 0;
		unsigned long long prevSystemUser_ = 0;
		unsigned long long prevProcessKernel_ = 0;
		unsigned long long prevProcessUser_ = 0;
	};
} // namespace Ken4lowEngine
