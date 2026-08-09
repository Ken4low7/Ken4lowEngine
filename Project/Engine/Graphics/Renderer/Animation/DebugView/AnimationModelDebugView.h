#pragma once

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class AnimationModel;


	/// -------------------------------------------------------------
	///				　アニメーションモデルデバッグビュー
	/// -------------------------------------------------------------
	class AnimationModelDebugView
	{
	public: /// ---------- メンバ関数 ---------- ///

		static void DrawImGui(AnimationModel& model);

		static void DrawSkeletonWireframe(AnimationModel& model);

		static void DrawBodyPartColliders(AnimationModel& model);
	};

}
