#include "GameplayTag.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace Ken4lowEngine
{

GameplayTag::GameplayTag(std::string name)
{
	if (IsValidString(name))
	{
		name_ = std::move(name);
	}
}

bool GameplayTag::IsValidString(std::string_view name)
{
	if (name.empty() || name.size() > 96u) return false;
	bool segmentHasCharacter = false;
	for (const char raw : name)
	{
		const unsigned char c = static_cast<unsigned char>(raw);
		if (raw == '.')
		{
			if (!segmentHasCharacter) return false;
			segmentHasCharacter = false;
			continue;
		}
		if (!(std::isalnum(c) != 0 || raw == '_' || raw == '-')) return false;
		segmentHasCharacter = true;
	}
	return segmentHasCharacter;
}

bool GameplayTag::Matches(const GameplayTag& query) const
{
	if (!IsValid() || !query.IsValid()) return false;
	if (name_ == query.name_) return true;
	if (name_.size() <= query.name_.size()) return false;
	return name_.compare(0u, query.name_.size(), query.name_) == 0 && name_[query.name_.size()] == '.';
}

bool GameplayTagContainer::Add(std::string_view name)
{
	return Add(GameplayTag{ std::string(name) });
}

bool GameplayTagContainer::Add(const GameplayTag& tag)
{
	if (!tag.IsValid()) return false;
	const auto it = std::lower_bound(tags_.begin(), tags_.end(), tag);
	if (it != tags_.end() && it->MatchesExact(tag)) return false;
	tags_.insert(it, tag);
	return true;
}

bool GameplayTagContainer::Remove(std::string_view name)
{
	const GameplayTag tag{ std::string(name) };
	if (!tag.IsValid()) return false;
	const auto it = std::lower_bound(tags_.begin(), tags_.end(), tag);
	if (it == tags_.end() || !it->MatchesExact(tag)) return false;
	tags_.erase(it);
	return true;
}

bool GameplayTagContainer::HasExact(std::string_view name) const
{
	const GameplayTag tag{ std::string(name) };
	if (!tag.IsValid()) return false;
	const auto it = std::lower_bound(tags_.begin(), tags_.end(), tag);
	return it != tags_.end() && it->MatchesExact(tag);
}

bool GameplayTagContainer::HasMatching(std::string_view query) const
{
	const GameplayTag queryTag{ std::string(query) };
	if (!queryTag.IsValid()) return false;
	for (const GameplayTag& tag : tags_)
	{
		if (tag.Matches(queryTag)) return true;
	}
	return false;
}

bool GameplayTagContainer::HasAll(const std::vector<GameplayTag>& queries) const
{
	for (const GameplayTag& query : queries)
	{
		bool matched = false;
		for (const GameplayTag& tag : tags_)
		{
			if (tag.Matches(query))
			{
				matched = true;
				break;
			}
		}
		if (!matched) return false;
	}
	return true;
}

bool GameplayTagContainer::HasAny(const std::vector<GameplayTag>& queries) const
{
	for (const GameplayTag& query : queries)
	{
		for (const GameplayTag& tag : tags_)
		{
			if (tag.Matches(query)) return true;
		}
	}
	return false;
}

} // namespace Ken4lowEngine
