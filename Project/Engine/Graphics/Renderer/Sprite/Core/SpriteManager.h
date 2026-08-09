#pragma once
#include "DX12Include.h"
#include "SpritePipelineSet.h"

namespace Ken4lowEngine
{
	class DirectXCommon;

	/// -------------------------------------------------------------
	///                 Sprite 描画の共通設定を行う管理クラス
	/// -------------------------------------------------------------
	/// このクラスは
	/// - Sprite 用 PipelineSet を初期化する
	/// - 描画前に RootSignature / PSO をコマンドリストへ設定する
	/// ことを担当する。
	///
	/// 実際の PSO 作成処理は SpritePipelineSet 側へ分離している。
	/// -------------------------------------------------------------
	class SpriteManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// シングルトンインスタンスを取得する。
		/// </summary>
		static SpriteManager* GetInstance();

		/// <summary>
		/// Sprite 描画に必要な共通オブジェクトを初期化する。
		/// </summary>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// 保持中の Sprite 関連リソース参照を解放する。
		/// </summary>
		void Finalize();

		/// <summary>
		/// 背景スプライト描画前の共通設定を行う。
		/// </summary>
		void SetRenderSetting_Background();

		/// <summary>
		/// UI スプライト描画前の共通設定を行う。
		/// </summary>
		void SetRenderSetting_UI();

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		/// Sprite 用 PSO / RootSignature のまとまり
		SpritePipelineSet pipelineSet_{};

	private: /// ---------- コピー禁止 ---------- ///

		SpriteManager() = default;
		~SpriteManager() = default;
		SpriteManager(const SpriteManager&) = delete;
		SpriteManager& operator=(const SpriteManager&) = delete;
	};
}