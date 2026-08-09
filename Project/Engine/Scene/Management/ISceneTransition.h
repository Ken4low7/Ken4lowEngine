#pragma once

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// シーン切り替え演出をSceneManagerへ注入するためのインターフェース
	/// -------------------------------------------------------------
	class ISceneTransition
	{
	public:
		virtual ~ISceneTransition() = default;

		virtual void Initialize() = 0;
		virtual void Update(float deltaTime) = 0;
		virtual void Draw2DSprites() = 0;
		virtual void DrawImGui() = 0;
		virtual void DrawInspectorContent() = 0;
		virtual void Finalize() = 0;

		virtual void StartCover() = 0;
		virtual void StartCrack() = 0;
		virtual bool IsFullyCovered() const = 0;
		virtual bool IsBusy() const = 0;
	};
}
