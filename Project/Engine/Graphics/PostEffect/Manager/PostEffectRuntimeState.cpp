#include "PostEffectRuntimeState.h"

namespace Ken4lowEngine
{
	void PostEffectRuntimeState::Clear()
	{
		editorEnabled_.clear();
		runtimeEnabled_.clear();
	}

	void PostEffectRuntimeState::RegisterEffect(const std::string& name, bool editorEnabledByDefault)
	{
		editorEnabled_[name] = editorEnabledByDefault;
		runtimeEnabled_[name] = false;
	}

	void PostEffectRuntimeState::SetEditorEnabled(const std::string& name, bool enabled)
	{
		editorEnabled_[name] = enabled;
	}

	void PostEffectRuntimeState::SetRuntimeEnabled(const std::string& name, bool enabled)
	{
		runtimeEnabled_[name] = enabled;
	}

	bool PostEffectRuntimeState::IsEditorEnabled(const std::string& name) const
	{
		const auto it = editorEnabled_.find(name);
		return it != editorEnabled_.end() && it->second;
	}

	bool PostEffectRuntimeState::IsRuntimeEnabled(const std::string& name) const
	{
		const auto it = runtimeEnabled_.find(name);
		return it != runtimeEnabled_.end() && it->second;
	}

	bool PostEffectRuntimeState::IsActive(const std::string& name) const
	{
		return IsEditorEnabled(name) || IsRuntimeEnabled(name); // 旧Managerの2つのmapをORする挙動を維持する。
	}
}
