#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{

/// <summary>
/// "State.Stunned"のような階層名でGameplay状態を表す軽量Tag。
/// Tagは大文字小文字を区別し、空Segmentや空白を許可しない。
/// </summary>
class GameplayTag
{
public:
	GameplayTag() = default;
	explicit GameplayTag(std::string name);

	static bool IsValidString(std::string_view name);

	[[nodiscard]] bool IsValid() const { return !name_.empty(); }
	[[nodiscard]] const std::string& GetName() const { return name_; }

	/// queryが"State"なら"State.Stunned"にも一致する。
	[[nodiscard]] bool Matches(const GameplayTag& query) const;
	[[nodiscard]] bool MatchesExact(const GameplayTag& other) const { return name_ == other.name_; }

	bool operator==(const GameplayTag&) const = default;
	bool operator<(const GameplayTag& other) const { return name_ < other.name_; }

private:
	std::string name_;
};

/// <summary>
/// 重複しないGameplayTag集合。検索順に依存しないよう内部は常にsort済みで保持する。
/// </summary>
class GameplayTagContainer
{
public:
	bool Add(std::string_view name);
	bool Add(const GameplayTag& tag);
	bool Remove(std::string_view name);
	void Clear() { tags_.clear(); }

	[[nodiscard]] bool HasExact(std::string_view name) const;
	[[nodiscard]] bool HasMatching(std::string_view query) const;
	[[nodiscard]] bool HasAll(const std::vector<GameplayTag>& queries) const;
	[[nodiscard]] bool HasAny(const std::vector<GameplayTag>& queries) const;
	[[nodiscard]] const std::vector<GameplayTag>& GetTags() const { return tags_; }

private:
	std::vector<GameplayTag> tags_;
};

} // namespace Ken4lowEngine
