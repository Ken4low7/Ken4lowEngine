#pragma once

#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	/// <summary>
	/// PostEffectのEditor有効状態とRuntime強制有効状態だけを管理します。<br/>
	/// 描画時は従来どおりEditorまたはRuntimeのどちらかがtrueならEffectを有効と判定します。
	/// </summary>
	class PostEffectRuntimeState
	{
	public:
		void Clear();
		void RegisterEffect(const std::string& name, bool editorEnabledByDefault);

		void SetEditorEnabled(const std::string& name, bool enabled);
		void SetRuntimeEnabled(const std::string& name, bool enabled);

		bool IsEditorEnabled(const std::string& name) const;
		bool IsRuntimeEnabled(const std::string& name) const;
		bool IsActive(const std::string& name) const;

	private:
		std::unordered_map<std::string, bool> editorEnabled_;
		std::unordered_map<std::string, bool> runtimeEnabled_;
	};
}
