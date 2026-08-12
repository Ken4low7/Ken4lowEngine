#pragma once

#include <GameTimer.h>
#include <Engine/Core/Streaming/StreamingManager.h>
#include <Engine/Scene/Streaming/WorldPartitionManager.h>
#include <Vector3.h>

#include <Windows.h>
#include <Psapi.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#pragma comment(lib, "Psapi.lib")

namespace Ken4lowEngine
{
	struct ReliabilityTelemetrySample
	{
		uint64_t frameIndex = 0;
		double elapsedSeconds = 0.0;
		double frameTimeMs = 0.0;
		double workingSetMB = 0.0;
		uint64_t frameAllocatedBytes = 0;
		std::size_t pendingStreamingRequests = 0;
		std::size_t queuedStreamingCompletions = 0;
		std::size_t loadedSubLevels = 0;
	};

	class ReliabilityTelemetry
	{
	public:
		static ReliabilityTelemetry* GetInstance()
		{
			static ReliabilityTelemetry instance;
			return &instance;
		}

		void InitializeFromEnvironment()
		{
			Finalize();
			initialized_ = true;
			const char* csvPath = std::getenv("KEN4LOW_RELIABILITY_CSV");
			const char* soakSeconds = std::getenv("KEN4LOW_SOAK_SECONDS");
			const char* streamingStress = std::getenv("KEN4LOW_STREAMING_STRESS");

			soakSeconds_ = ParsePositiveDouble(soakSeconds).value_or(0.0);
			streamingStressEnabled_ = streamingStress != nullptr && std::string(streamingStress) != "0";
			startTime_ = Clock::now();
			frameIndex_ = 0;

			if (csvPath == nullptr || *csvPath == '\0')
			{
				enabled_ = soakSeconds_ > 0.0 || streamingStressEnabled_;
				return;
			}

			csvPath_ = std::filesystem::path(csvPath);
			std::error_code error;
			if (csvPath_.has_parent_path())
			{
				std::filesystem::create_directories(csvPath_.parent_path(), error);
			}
			stream_.open(csvPath_, std::ios::trunc);
			enabled_ = stream_.is_open() || soakSeconds_ > 0.0 || streamingStressEnabled_;
			if (stream_.is_open())
			{
				// Stable CSV columns make the same capture consumable by leak, performance, streaming and soak gates.
				stream_ << "frame,elapsed_seconds,frame_time_ms,working_set_mb,frame_allocated_bytes,"
					"pending_streaming,queued_completions,loaded_sublevels\n";
			}
		}

		void Finalize()
		{
			if (stream_.is_open())
			{
				stream_.flush();
				stream_.close();
			}
			initialized_ = false;
			enabled_ = false;
			soakSeconds_ = 0.0;
			streamingStressEnabled_ = false;
			frameIndex_ = 0;
			csvPath_.clear();
		}

		void RecordCurrentFrame(uint64_t frameAllocatedBytes)
		{
			if (!initialized_)
			{
				InitializeFromEnvironment();
			}
			if (!enabled_)
			{
				return;
			}

			StreamingManager* streaming = StreamingManager::GetInstance();
			WorldPartitionManager* worldPartition = WorldPartitionManager::GetInstance();
			RecordFrame(
				static_cast<double>(GameTimer::GetInstance()->GetDeltaTime()) * 1000.0,
				frameAllocatedBytes,
				streaming->GetPendingRequestCount(),
				streaming->GetQueuedCompletionCount(),
				worldPartition->GetLoadedSubLevelCount());

			if (streamingStressEnabled_ && worldPartition->IsConfigured() && worldPartition->IsEnabled())
			{
				const auto [offsetX, offsetZ] = GetStreamingStressOffset(worldPartition->GetSettings().cellSize);
				Vector3 source = worldPartition->GetStreamingSourcePosition();
				source.x += offsetX;
				source.z += offsetZ;
				worldPartition->Update(source);
			}

			if (ShouldStopSoak())
			{
				PostQuitMessage(0);
			}
		}

		void RecordFrame(
			double frameTimeMs,
			uint64_t frameAllocatedBytes,
			std::size_t pendingStreamingRequests,
			std::size_t queuedStreamingCompletions,
			std::size_t loadedSubLevels)
		{
			if (!enabled_)
			{
				return;
			}

			ReliabilityTelemetrySample sample{};
			sample.frameIndex = frameIndex_++;
			sample.elapsedSeconds = GetElapsedSeconds();
			sample.frameTimeMs = frameTimeMs;
			sample.workingSetMB = QueryWorkingSetMB();
			sample.frameAllocatedBytes = frameAllocatedBytes;
			sample.pendingStreamingRequests = pendingStreamingRequests;
			sample.queuedStreamingCompletions = queuedStreamingCompletions;
			sample.loadedSubLevels = loadedSubLevels;

			if (stream_.is_open())
			{
				stream_ << sample.frameIndex << ','
					<< sample.elapsedSeconds << ','
					<< sample.frameTimeMs << ','
					<< sample.workingSetMB << ','
					<< sample.frameAllocatedBytes << ','
					<< sample.pendingStreamingRequests << ','
					<< sample.queuedStreamingCompletions << ','
					<< sample.loadedSubLevels << '\n';
				if ((sample.frameIndex % 120u) == 0u)
				{
					stream_.flush();
				}
			}
		}

		[[nodiscard]] bool IsEnabled() const { return enabled_; }
		[[nodiscard]] bool IsStreamingStressEnabled() const { return streamingStressEnabled_; }
		[[nodiscard]] bool ShouldStopSoak() const { return soakSeconds_ > 0.0 && GetElapsedSeconds() >= soakSeconds_; }

		[[nodiscard]] std::pair<float, float> GetStreamingStressOffset(float cellSize) const
		{
			if (!streamingStressEnabled_ || cellSize <= 0.0f)
			{
				return { 0.0f, 0.0f };
			}

			constexpr int kRoute[][2] = {
				{ 0, 0 }, { 4, 0 }, { 4, 4 }, { 0, 4 },
				{ -4, 4 }, { -4, 0 }, { -4, -4 }, { 0, -4 }, { 4, -4 },
			};
			constexpr std::size_t kRouteCount = sizeof(kRoute) / sizeof(kRoute[0]);
			constexpr uint64_t kFramesPerPoint = 120;
			const std::size_t routeIndex = static_cast<std::size_t>((frameIndex_ / kFramesPerPoint) % kRouteCount);
			return {
				static_cast<float>(kRoute[routeIndex][0]) * cellSize,
				static_cast<float>(kRoute[routeIndex][1]) * cellSize,
			};
		}

	private:
		using Clock = std::chrono::steady_clock;

		ReliabilityTelemetry() = default;
		~ReliabilityTelemetry() { Finalize(); }

		[[nodiscard]] double GetElapsedSeconds() const
		{
			return std::chrono::duration<double>(Clock::now() - startTime_).count();
		}

		[[nodiscard]] static std::optional<double> ParsePositiveDouble(const char* text)
		{
			if (text == nullptr || *text == '\0')
			{
				return std::nullopt;
			}
			char* end = nullptr;
			const double value = std::strtod(text, &end);
			if (end == text || value <= 0.0)
			{
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] static double QueryWorkingSetMB()
		{
			PROCESS_MEMORY_COUNTERS counters{};
			if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
			{
				return 0.0;
			}
			constexpr double kBytesPerMegabyte = 1024.0 * 1024.0;
			return static_cast<double>(counters.WorkingSetSize) / kBytesPerMegabyte;
		}

		bool initialized_ = false;
		bool enabled_ = false;
		bool streamingStressEnabled_ = false;
		double soakSeconds_ = 0.0;
		uint64_t frameIndex_ = 0;
		Clock::time_point startTime_ = Clock::now();
		std::filesystem::path csvPath_;
		std::ofstream stream_;
	};
} // namespace Ken4lowEngine
