#pragma once

#include "Engine/Graphics/Common/BlendModeType.h"
#include "Engine/Graphics/Material/Material.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	enum class ForwardRenderBucket : uint8_t
	{
		Opaque = 0,
		Masked,
		Transparent,
		Additive,
		Count,
	};

	inline constexpr size_t kForwardRenderBucketCount = static_cast<size_t>(ForwardRenderBucket::Count);

	enum class ForwardSortMode : uint8_t
	{
		FrontToBack = 0,
		BackToFront,
	};

	/// <summary>
	/// Materialの高レベルなBlend分類からForward passの不変条件を決定します。
	/// </summary>
	struct ForwardRenderPolicy
	{
		ForwardRenderBucket bucket = ForwardRenderBucket::Opaque;
		BlendMode blendMode = BlendMode::kBlendModeNone;
		ForwardSortMode sortMode = ForwardSortMode::FrontToBack;
		bool depthWrite = true;
		bool alphaTest = false;
	};

	constexpr ForwardRenderPolicy ResolveForwardRenderPolicy(MaterialBlendMode blendMode)
	{
		switch (blendMode)
		{
		case MaterialBlendMode::Masked:
			return { ForwardRenderBucket::Masked, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, true };
		case MaterialBlendMode::Transparent:
			return { ForwardRenderBucket::Transparent, BlendMode::kBlendModeNormal, ForwardSortMode::BackToFront, false, false };
		case MaterialBlendMode::Additive:
			return { ForwardRenderBucket::Additive, BlendMode::kBlendModeAdd, ForwardSortMode::BackToFront, false, false };
		case MaterialBlendMode::Opaque:
		default:
			return { ForwardRenderBucket::Opaque, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, false };
		}
	}

	constexpr bool IsForwardTransparentBucket(ForwardRenderBucket bucket)
	{
		return bucket == ForwardRenderBucket::Transparent || bucket == ForwardRenderBucket::Additive;
	}

	constexpr const char* GetForwardRenderBucketName(ForwardRenderBucket bucket)
	{
		switch (bucket)
		{
		case ForwardRenderBucket::Opaque: return "Opaque";
		case ForwardRenderBucket::Masked: return "Masked";
		case ForwardRenderBucket::Transparent: return "Transparent";
		case ForwardRenderBucket::Additive: return "Additive";
		case ForwardRenderBucket::Count:
		default:
			return "Unknown";
		}
	}

	using ForwardRenderDrawCallback = void(*)(void* payload);

	struct ForwardRenderItem
	{
		void* payload = nullptr;
		ForwardRenderDrawCallback draw = nullptr;
		ForwardRenderPolicy policy{};
		float sortDepth = 0.0f;
		uint64_t submissionOrder = 0;
	};

	inline ForwardRenderItem MakeForwardRenderItem(
		void* payload,
		ForwardRenderDrawCallback draw,
		MaterialBlendMode blendMode,
		float sortDepth)
	{
		ForwardRenderItem item{};
		item.payload = payload;
		item.draw = draw;
		item.policy = ResolveForwardRenderPolicy(blendMode); // Renderer種類に依存せずMaterial分類から同じQueue契約を生成する。
		item.sortDepth = sortDepth;
		return item;
	}

	/// <summary>
	/// EndFrame後もEditor/Validationから参照できる、直近Forward Frameの軽量統計です。
	/// </summary>
	struct ForwardRenderFrameStats
	{
		uint64_t frameSerial = 0;
		std::array<size_t, kForwardRenderBucketCount> submittedByBucket{};
		std::array<bool, kForwardRenderBucketCount> executedByBucket{};
		std::array<ForwardRenderBucket, kForwardRenderBucketCount> executionOrder{};
		size_t executionCount = 0;
		size_t totalSubmitted = 0;
		size_t rejectedSubmissions = 0;
		size_t duplicateExecutionRequests = 0;
		bool canonicalExecutionOrder = false;
	};

	constexpr bool IsCanonicalForwardExecutionOrder(
		const std::array<ForwardRenderBucket, kForwardRenderBucketCount>& executionOrder,
		size_t executionCount)
	{
		constexpr std::array<ForwardRenderBucket, kForwardRenderBucketCount> kExpectedOrder = {
			ForwardRenderBucket::Opaque,
			ForwardRenderBucket::Masked,
			ForwardRenderBucket::Transparent,
			ForwardRenderBucket::Additive,
		};

		if (executionCount != kExpectedOrder.size()) return false;
		for (size_t index = 0; index < kExpectedOrder.size(); ++index)
		{
			if (executionOrder[index] != kExpectedOrder[index]) return false;
		}
		return true;
	}

	/// <summary>
	/// 1回のScene 3D描画でForward Itemを収集し、Bucket契約に従って安定順序で実行するCPU Queueです。
	/// </summary>
	class ForwardRenderQueue
	{
	public:
		static ForwardRenderQueue* GetInstance()
		{
			static ForwardRenderQueue instance;
			return &instance;
		}

		void BeginFrame()
		{
			for (auto& bucket : buckets_)
			{
				bucket.clear();
			}
			bucketExecuted_.fill(false);
			executionOrder_.fill(ForwardRenderBucket::Count);
			executionCount_ = 0;
			rejectedSubmissionCount_ = 0;
			duplicateExecutionRequestCount_ = 0;
			nextSubmissionOrder_ = 0;
			++frameSerial_;
			if (frameSerial_ == 0) ++frameSerial_;
			frameActive_ = true; // Queue所有期間をActorWorld::Draw内へ限定し、通常の直接Drawへ状態を漏らさない。
		}

		void EndFrame()
		{
			CaptureLastFrameStats(); // EndFrame後もEditor/CI診断から直近のBucket状態を確認できるようSnapshotを残す。
			frameActive_ = false;
		}

		bool Submit(ForwardRenderItem item)
		{
			if (!frameActive_ || item.payload == nullptr || item.draw == nullptr)
			{
				++rejectedSubmissionCount_;
				return false;
			}

			const size_t bucketIndex = static_cast<size_t>(item.policy.bucket);
			if (bucketIndex >= kBucketCount)
			{
				++rejectedSubmissionCount_;
				return false;
			}

			item.submissionOrder = nextSubmissionOrder_++;
			buckets_[bucketIndex].push_back(item);
			return true;
		}

		void ExecuteBucket(ForwardRenderBucket bucket)
		{
			if (!frameActive_)
			{
				return;
			}

			const size_t bucketIndex = static_cast<size_t>(bucket);
			if (bucketIndex >= kBucketCount)
			{
				return;
			}

			if (bucketExecuted_[bucketIndex])
			{
				++duplicateExecutionRequestCount_;
				return; // 同一Bucketの二重実行は半透明の二重合成へ直結するため、診断値を残してDraw自体を抑止する。
			}

			if (executionCount_ < executionOrder_.size())
			{
				executionOrder_[executionCount_++] = bucket;
			}

			auto& items = buckets_[bucketIndex];
			if (!items.empty())
			{
				const ForwardSortMode sortMode = items.front().policy.sortMode;
				std::stable_sort(items.begin(), items.end(),
					[sortMode](const ForwardRenderItem& lhs, const ForwardRenderItem& rhs)
					{
						if (lhs.sortDepth == rhs.sortDepth)
						{
							return lhs.submissionOrder < rhs.submissionOrder;
						}
						return sortMode == ForwardSortMode::BackToFront
							? lhs.sortDepth > rhs.sortDepth
							: lhs.sortDepth < rhs.sortDepth;
					});

				for (const ForwardRenderItem& item : items)
				{
					item.draw(item.payload);
				}
			}

			bucketExecuted_[bucketIndex] = true;
		}

		bool WasBucketExecuted(ForwardRenderBucket bucket) const
		{
			const size_t bucketIndex = static_cast<size_t>(bucket);
			return frameActive_ && bucketIndex < kBucketCount && bucketExecuted_[bucketIndex];
		}

		size_t GetSubmittedCount(ForwardRenderBucket bucket) const
		{
			const size_t bucketIndex = static_cast<size_t>(bucket);
			return bucketIndex < kBucketCount ? buckets_[bucketIndex].size() : 0;
		}

		bool OwnsPayload(const void* payload) const
		{
			if (!frameActive_ || payload == nullptr)
			{
				return false;
			}
			for (const auto& bucket : buckets_)
			{
				for (const ForwardRenderItem& item : bucket)
				{
					if (item.payload == payload) return true; // 実行済みBucketもFrame終了まで所有し、Actorの通常Drawとの二重実行を防ぐ。
				}
			}
			return false;
		}

		const ForwardRenderFrameStats& GetLastFrameStats() const { return lastFrameStats_; }
		bool WasLastFrameExecutionOrderCanonical() const { return lastFrameStats_.canonicalExecutionOrder; }
		uint64_t GetFrameSerial() const { return frameSerial_; }
		bool IsFrameActive() const { return frameActive_; }

	private:
		static constexpr size_t kBucketCount = kForwardRenderBucketCount;

		void CaptureLastFrameStats()
		{
			ForwardRenderFrameStats stats{};
			stats.frameSerial = frameSerial_;
			stats.executedByBucket = bucketExecuted_;
			stats.executionOrder = executionOrder_;
			stats.executionCount = executionCount_;
			stats.rejectedSubmissions = rejectedSubmissionCount_;
			stats.duplicateExecutionRequests = duplicateExecutionRequestCount_;
			for (size_t index = 0; index < kBucketCount; ++index)
			{
				stats.submittedByBucket[index] = buckets_[index].size();
				stats.totalSubmitted += stats.submittedByBucket[index];
			}
			stats.canonicalExecutionOrder = IsCanonicalForwardExecutionOrder(stats.executionOrder, stats.executionCount);
			lastFrameStats_ = stats;
		}

		ForwardRenderQueue() = default;
		std::array<std::vector<ForwardRenderItem>, kBucketCount> buckets_{};
		std::array<bool, kBucketCount> bucketExecuted_{};
		std::array<ForwardRenderBucket, kBucketCount> executionOrder_{};
		ForwardRenderFrameStats lastFrameStats_{};
		size_t executionCount_ = 0;
		size_t rejectedSubmissionCount_ = 0;
		size_t duplicateExecutionRequestCount_ = 0;
		uint64_t nextSubmissionOrder_ = 0;
		uint64_t frameSerial_ = 0;
		bool frameActive_ = false;
	};
} // namespace Ken4lowEngine
