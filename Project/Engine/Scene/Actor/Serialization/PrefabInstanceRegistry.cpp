#include "PrefabInstanceRegistry.h"

#include "Actor.h"

namespace Ken4lowEngine
{
	PrefabInstanceRegistry* PrefabInstanceRegistry::GetInstance()
	{
		static PrefabInstanceRegistry instance;
		return &instance;
	}

	void PrefabInstanceRegistry::Register(const Actor* actor, std::string_view prefabPath)
	{
		if (!actor || prefabPath.empty()) return;
		std::scoped_lock lock(mutex_);
		prefabPathByActor_[actor] = std::string(prefabPath);
	}

	bool PrefabInstanceRegistry::Find(const Actor* actor, std::string& outPrefabPath) const
	{
		if (!actor) return false;
		std::scoped_lock lock(mutex_);
		const auto found = prefabPathByActor_.find(actor);
		if (found == prefabPathByActor_.end()) return false;
		outPrefabPath = found->second;
		return true;
	}

	void PrefabInstanceRegistry::Remove(const Actor* actor)
	{
		if (!actor) return;
		std::scoped_lock lock(mutex_);
		prefabPathByActor_.erase(actor);
	}
} // namespace Ken4lowEngine
