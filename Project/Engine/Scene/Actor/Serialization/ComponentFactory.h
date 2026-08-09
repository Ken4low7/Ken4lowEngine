#pragma once
#include <functional>
#include <string_view>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class Actor;
	class ActorComponent;
	class SceneComponent;

	/// -------------------------------------------------------------
	///		  JSONのClass文字列からComponentを生成するFactory
	/// -------------------------------------------------------------
	class ComponentFactory
	{
	public: /// ---------- 構造体 ---------- ///

		/// ---------- コンポーネント群の情報を保持する構造体 ---------- ///
		struct ComponentTypeInfo
		{
			std::string className;									// JSON保存用のComponentクラス名
			std::string displayName;								// ImGui表示用のComponent名
			std::string category;									// ImGui上で分類するためのカテゴリ名
			std::string description;								// Componentの役割を説明する表示文
			bool allowMultiple;										// 同一Actorに複数追加可能かどうか
			bool canBeRoot;											// RootComponentとして生成できるかどうか
			std::function<ActorComponent* (Actor*)> createFunc;		// Component生成関数
			std::function<SceneComponent* (Actor*)> createRootFunc; // RootComponent生成関数
		};

	public: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// 指定されたClass名に対応するComponentをActorへ追加する
		/// </summary>
		static ActorComponent* CreateComponent(Actor* owner, std::string_view className);

		/// <summary>
		/// 指定されたClass名に対応するSceneComponentをRootComponentとして生成する
		/// </summary>
		static SceneComponent* CreateRootSceneComponent(Actor* owner, std::string_view className);

		/// <summary>
		/// Add Component UIに表示するComponent一覧を取得する
		/// </summary>
		static const std::vector<ComponentTypeInfo>& GetRegisteredComponentTypes();

		/// ApplicationLayer固有ComponentをEditor追加とActor JSON復元の対象へ登録する。
		static void RegisterComponentType(ComponentTypeInfo typeInfo);

		/// <summary>
		/// 指定したComponentが同一Actorに複数追加可能かどうかを取得する
		/// </summary>
		static bool IsAllowMultiple(std::string_view className);

	private: /// ---------- 静的メンバ関数 ---------- ///

		/// <summary>
		/// 登録済みComponent一覧から。Class名が一致するComponent情報を検索する
		/// </summary>
		static const ComponentTypeInfo* FindComponentType(std::string_view className);
	};
}
