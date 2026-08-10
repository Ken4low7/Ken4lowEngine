#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Ken4lowEngine
{
	class Actor;

	/// <summary>
	/// Level内のActorがどのPrefabから生成されたかをRuntime pointerへ紐付ける軽量Registry。
	/// Actor本体のJSONには混ぜず、LevelSerializerがPrefab参照とOverrideを保存するときだけ利用する。
	/// </summary>
	class PrefabInstanceRegistry
	{
	public:
		static PrefabInstanceRegistry* GetInstance();

		void Register(const Actor* actor, std::string_view prefabPath);
		bool Find(const Actor* actor, std::string& outPrefabPath) const;
		void Remove(const Actor* actor);

	private:
		PrefabInstanceRegistry() = default;
		~PrefabInstanceRegistry() = default;
		PrefabInstanceRegistry(const PrefabInstanceRegistry&) = delete;
		PrefabInstanceRegistry& operator=(const PrefabInstanceRegistry&) = delete;

		mutable std::mutex mutex_;
		std::unordered_map<const Actor*, std::string> prefabPathByActor_;
	};
} // namespace Ken4lowEngine
