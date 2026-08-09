#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace Ken4lowEngine
{
	class Actor;

	/// <summary>
	/// World Outliner専用の表示、ロック、フォルダー状態をActor本体から分離して保持します。
	/// </summary>
	struct EditorActorState
	{
		bool visible = true;
		bool locked = false;
		std::string folderPath;
	};

	/// <summary>
	/// Scene編集中だけ必要なActor状態をActorポインタ単位で管理します。
	/// </summary>
	class EditorActorStateRegistry
	{
	public:
		static EditorActorStateRegistry* GetInstance()
		{
			static EditorActorStateRegistry instance;
			return &instance;
		}

		const EditorActorState& GetState(const Actor* actor) const
		{
			const auto found = states_.find(actor);
			return found != states_.end() ? found->second : defaultState_;
		}

		EditorActorState& GetOrCreateState(const Actor* actor)
		{
			return states_[actor]; // 初回参照時はEditor既定値で状態を生成する。
		}

		bool IsVisible(const Actor* actor) const { return GetState(actor).visible; }
		bool IsLocked(const Actor* actor) const { return GetState(actor).locked; }
		const std::string& GetFolderPath(const Actor* actor) const { return GetState(actor).folderPath; }

		void SetVisible(const Actor* actor, bool visible) { GetOrCreateState(actor).visible = visible; }
		void SetLocked(const Actor* actor, bool locked) { GetOrCreateState(actor).locked = locked; }
		void SetFolderPath(const Actor* actor, std::string_view folderPath)
		{
			GetOrCreateState(actor).folderPath.assign(folderPath.begin(), folderPath.end());
		}

		void SetState(const Actor* actor, const EditorActorState& state) { states_[actor] = state; }
		void Remove(const Actor* actor) { states_.erase(actor); }
		void Clear() { states_.clear(); }

	private:
		EditorActorStateRegistry() = default;
		EditorActorStateRegistry(const EditorActorStateRegistry&) = delete;
		EditorActorStateRegistry& operator=(const EditorActorStateRegistry&) = delete;

		std::unordered_map<const Actor*, EditorActorState> states_;
		EditorActorState defaultState_{};
	};
} // namespace Ken4lowEngine
