#pragma once
#include "PipelineCommon.h"

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class PipelineFactory;
	class DXCCompilerManager;

	/// -------------------------------------------------------------
	///                Sprite 描画用パイプラインをまとめるクラス
	/// -------------------------------------------------------------
	/// このクラスは Sprite 描画で使用する
	/// - RootSignature
	/// - PipelineState
	/// を用途別に保持する。
	///
	/// 現在は
	/// - 背景用
	/// - UI 用
	/// の 2 種類を管理する。
	///
	/// 注意:
	/// 実際にコマンドリストへ設定する責務は持たず、
	/// PSO / RootSignature の生成と保持だけを担当する。
	/// 描画時のバインドは SpriteManager 側で行う。
	/// -------------------------------------------------------------
	class SpritePipelineSet
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// Sprite 用パイプライン群を初期化する。
		/// </summary>
		/// <param name="factory">
		/// RootSignature / PipelineState の生成を行うファクトリ。
		/// </param>
		/// <param name="dxcManager">
		/// シェーダーコンパイルに使用する DXC 管理クラス。
		/// </param>
		/// <param name="rtvFormat">
		/// 描画先 RTV のフォーマット。
		/// </param>
		/// <param name="dsvFormat">
		/// 使用する DSV のフォーマット。
		/// </param>
		void Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat);

		/// <summary>
		/// 保持している Sprite 用パイプラインを解放する。
		/// </summary>
		void Finalize();

	public: /// ---------- アクセッサ ---------- ///

		/// <summary>
		/// 背景スプライト描画用のパイプラインを取得する。
		/// </summary>
		const PipelineBundle& GetBackground() const { return backgroundPipeline_; }

		/// <summary>
		/// UI スプライト描画用のパイプラインを取得する。
		/// </summary>
		const PipelineBundle& GetUI() const { return uiPipeline_; }

	private: /// ---------- メンバ変数 ---------- ///

		/// 背景スプライト描画用
		PipelineBundle backgroundPipeline_{};

		/// UI スプライト描画用
		PipelineBundle uiPipeline_{};
	};
}