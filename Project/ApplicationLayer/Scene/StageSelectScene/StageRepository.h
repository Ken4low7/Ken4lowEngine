#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>
#include "IStageSelector.h"

/// -------------------------------------------------------------
///				ステージ情報リポジトリ（シングルトン）
/// -------------------------------------------------------------
class StageRepository
{
public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス取得
	static StageRepository& GetInstance() { static StageRepository inst; return inst; }

	// ステージ情報リストの設定・取得
	void SetStages(std::vector<StageInfo> stages)
	{
		stages_ = std::move(stages);
		ResolveStartIndexFromStageId(); // ステージ配列を再構築しても選択済みIDから同じステージを復元する。
	}
	const std::vector<StageInfo>& GetStages() const { return stages_; }

	// スタートインデックスの設定・取得
	void SetStartIndex(int idx)
	{
		startIndex_ = idx;
		if (idx >= 0 && idx < static_cast<int>(stages_.size()))
		{
			selectedStageId_ = stages_[static_cast<size_t>(idx)].id;
		}
	}
	std::optional<int> GetStartIndex() const
	{
		if (selectedStageId_)
		{
			const auto it = std::find_if(stages_.begin(), stages_.end(), [this](const StageInfo& stage)
				{
					return stage.id == *selectedStageId_;
				});
			if (it != stages_.end())
			{
				return static_cast<int>(std::distance(stages_.begin(), it));
			}
		}
		return startIndex_;
	}
	void ClearStartIndex()
	{
		startIndex_.reset();
		selectedStageId_.reset();
	}

private: /// ---------- メンバ関数 ---------- ///

	void ResolveStartIndexFromStageId()
	{
		if (!selectedStageId_) return;
		const auto it = std::find_if(stages_.begin(), stages_.end(), [this](const StageInfo& stage)
			{
				return stage.id == *selectedStageId_;
			});
		if (it != stages_.end())
		{
			startIndex_ = static_cast<int>(std::distance(stages_.begin(), it));
		}
	}

private: /// ---------- メンバ変数 ---------- ///

	StageRepository() = default;
	std::vector<StageInfo> stages_;
	std::optional<int> startIndex_;
	std::optional<uint32_t> selectedStageId_;
};
