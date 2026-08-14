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

	using ForwardRenderDrawCallback = void(*)(void* payload);

	struct ForwardRenderItem
	{
		void* payload = nullptr;
		ForwardRenderDrawCallback draw = nullptr;
		ForwardRenderPolicy policy{};
		float sortDepth = 0.0f;
		uint64_t submissionOrder = 0;
	};

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
			nextSubmissionOrder_ = 0;
			++frameSerial_;
			if (frameSerial_ == 0) ++frameSerial_;
			frameActive_ = true; // Queue所有期間をActorWorld::Draw内へ限定し、通常の直接Drawへ状態を漏らさない。
		}

		void EndFrame()
		{
			frameActive_ = false;
		}

		bool Submit(ForwardRenderItem item)
		{
			if (!frameActive_ || item.payload == nullptr || item.draw == nullptr)
			{
				return false;
			}

			const size_t bucketIndex = static_cast<size_t>(item.policy.bucket);
			if (bucketIndex >= kBucketCount)
			{
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

		uint64_t GetFrameSerial() const { return frameSerial_; }
		bool IsFrameActive() const { return frameActive_; }

	private:
		static constexpr size_t kBucketCount = static_cast<size_t>(ForwardRenderBucket::Count);

		ForwardRenderQueue() = default;
		std::array<std::vector<ForwardRenderItem>, kBucketCount> buckets_{};
		std::array<bool, kBucketCount> bucketExecuted_{};
		uint64_t nextSubmissionOrder_ = 0;
		uint64_t frameSerial_ = 0;
		bool frameActive_ = false;
	};
} // namespace Ken4lowEngine
