#include "PostEffectChain.h"

#include <algorithm>

namespace Ken4lowEngine
{
	void PostEffectChain::Clear()
	{
		effectOrder_.clear();
	}

	void PostEffectChain::RegisterEffect(const std::string& name, int order)
	{
		// effectTableのorder値をそのまま登録し、既存のPostEffect適用順を保持する。
		effectOrder_.emplace_back(name, order);
	}

	std::vector<std::string> PostEffectChain::GetOrderedEffectNames() const
	{
		std::vector<std::pair<std::string, int>> sortedEffects = effectOrder_;
		std::sort(sortedEffects.begin(), sortedEffects.end(),
			[](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });

		std::vector<std::string> orderedNames;
		orderedNames.reserve(sortedEffects.size());
		for (const auto& [name, order] : sortedEffects)
		{
			(void)order;
			orderedNames.push_back(name); // Chainは順序だけを返し、有効状態を参照しない。
		}
		return orderedNames;
	}
}
