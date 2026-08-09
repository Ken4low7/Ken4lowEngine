#pragma once
#include <Actor.h>

#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		 文字列のActorクラス名からActorを生成するFactory
	/// -------------------------------------------------------------
	class ActorFactory
	{
	public: /// ---------- テンプレート関数 ---------- ///

		/// <summary>
		/// Actorを生成する関数型
		/// </summary>
		using CreatorFunc = std::function<std::unique_ptr<Actor>()>;

		/// <summary>
		/// 指定型Actorを生成対象として登録する
		/// </summary>
		template<class T>
		static void RegisterActorClass(std::string_view className)
		{
			static_assert(std::is_base_of_v<Actor, T>, "T must inherit from Actor.");

			RegisterActor(className, []()
			{
				return std::make_unique<T>(); // 登録されたActor型を生成する
			});
		}

	public: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// Actor生成関数を登録する
		/// </summary>
		static void RegisterActor(std::string_view className, CreatorFunc creator);

		/// <summary>
		/// 指定されたClass名に対応するActorを生成する
		/// </summary>
		static std::unique_ptr<Actor> CreateActor(std::string_view className);

		/// <summary>
		/// 指定Class名が登録済みか確認する。
		/// </summary>
		static bool IsRegistered(std::string_view className);
	};
}