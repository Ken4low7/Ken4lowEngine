#include "ActorFactory.h"
#include "../Character/CharacterActor.h"

#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Actor生成関数の登録テーブルを取得する
		/// </summary>
		std::unordered_map<std::string, ActorFactory::CreatorFunc>& GetActorFactoryRegister()
		{
			static std::unordered_map<std::string, ActorFactory::CreatorFunc> registry; // Actor生成関数の登録マップ
			return registry;
		}
	}

	void ActorFactory::RegisterActor(std::string_view className, CreatorFunc creator)
	{
		if (className.empty() || !creator)
		{
			return; // 無効なクラス名または生成関数なら登録しない
		}

		GetActorFactoryRegister()[std::string(className)] = std::move(creator); // クラス名と生成関数を登録する
	}

	std::unique_ptr<Actor> Ken4lowEngine::ActorFactory::CreateActor(std::string_view className)
	{
		if (className == "Actor")
		{
			// EditorがModelComponent構成から生成した汎用Actorは、個別登録なしでJSONから復元する。
			return std::make_unique<Actor>();
		}
		if (className == "CharacterActor")
		{
			return std::make_unique<CharacterActor>(); // Character共通基底はFactory組み込み型としてJSON復元を保証する。
		}

		const auto& registry = GetActorFactoryRegister();

		const auto it = registry.find(std::string(className));
		if (it == registry.end())
		{
			return nullptr; // 登録されていないクラス名ならnullptrを返す
		}

		return it->second(); // 登録された生成関数を呼び出してActorを生成する
	}

	bool ActorFactory::IsRegistered(std::string_view className)
	{
		if (className == "Actor" || className == "CharacterActor")
		{
			return true; // Engine組み込みActorは個別登録なしで常に生成可能にする。
		}

		const auto& registry = GetActorFactoryRegister();
		return registry.contains(std::string(className)); // 登録済みかどうかを返す
	}

} // namespace Ken4lowEngine
