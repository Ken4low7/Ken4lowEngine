#pragma once

#include "IPostEffect.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class PostEffectPipelineBuilder;
	class PostEffectRuntimeState;

	enum class PostEffectExecutionPath
	{
		Graphics,
		Compute,
	};

	/// <summary>登録名、既定状態、適用順、カテゴリ、実行Pathを保持するEffect定義です。</summary>
	struct PostEffectDefinition
	{
		std::string name;
		bool editorEnabledByDefault = false;
		int order = 0;
		std::string category;
		PostEffectExecutionPath executionPath = PostEffectExecutionPath::Graphics;
	};

	/// <summary>
	/// Built-in PostEffectの登録・生成・所有・破棄を担当します。<br/>
	/// RenderTarget、Barrier、実行順の並べ替え、有効状態には触れません。
	/// </summary>
	class PostEffectRegistry
	{
	public:
		void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* pipelineBuilder);
		void Finalize();
		void UpdateEditorEnabledEffects(const PostEffectRuntimeState& runtimeState);

		IPostEffect* Find(const std::string& name);
		const IPostEffect* Find(const std::string& name) const;
		const PostEffectDefinition* FindDefinition(const std::string& name) const;
		const std::vector<PostEffectDefinition>& GetDefinitions() const { return definitions_; }

	private:
		struct RegisteredEffect
		{
			PostEffectDefinition definition;
			std::unique_ptr<IPostEffect> effect;
		};

		std::vector<PostEffectDefinition> definitions_;
		std::vector<RegisteredEffect> effects_;
	};
}
