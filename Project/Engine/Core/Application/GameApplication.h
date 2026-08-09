#pragma once
#include <Framework.h>
#include "RenderPipelineController.h"

#include <memory>

namespace Ken4lowEngine
{
	/// ---------- 前方宣言 ---------- ///
	class SceneManager;

	/// -------------------------------------------------------------
	///				　	ゲーム全体を管理するクラス
	/// -------------------------------------------------------------
	class GameApplication : public Framework
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SceneManagerの完全型が見えるcpp側で生成処理を定義します。
		/// </summary>
		GameApplication();

		/// <summary>
		/// デストラクタ
		/// </summary>
		~GameApplication() override;

		/// <summary>
		/// ゲーム全体の初期化処理。
		/// まず Framework::Initialize() を呼び出して、ウィンドウ・DirectX・各種マネージャなど
		/// 基盤部分を初期化したあと、
		/// ・Input の初期化
		/// ・ParameterManager によるグローバルパラメータの読み込み
		/// ・SceneFactory の生成と SceneManager への登録
		/// ・最初のシーン（TitleScene）の設定
		/// を行います。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// 毎フレームの更新処理。
		/// Framework::Update() で共通の更新（カメラや各マネージャの更新前処理など）を行ったあと、
		/// ・入力状態の更新（Input）
		/// ・DebugCamera/通常カメラの更新切り替え
		/// ・SceneManager の更新（現在シーンの Update）
		/// ・PostEffectManager の更新
		/// などを行い、ゲーム全体の状態を進めます。
		/// </summary>
		void Update() override;

		/// <summary>
		/// 毎フレームの描画処理。
		/// おおまかな流れは以下の通りです：
		/// 既存public API互換の入口として残し、実際のShadow/Scene/PostEffect/UIの順序管理は
		/// RenderPipelineControllerへ委譲します。
		/// </summary>
		void Draw() override;

		/// <summary>
		/// ゲーム終了時の後始末処理。
		/// SceneManager側の終了処理と自動破棄を先に完了させ、
		/// その後にFramework側の描画基盤を終了します。
		/// </summary>
		void Finalize() override;

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 現在シーンの3Dオブジェクト、ワイヤーフレーム、GPU/CPUパーティクルを決まった順番で描画します。
		/// </summary>
		// Debug/Releaseでゲーム本編の3D描画順を揃えるための共通ルート。
		void DrawCurrentScene3DPass();

		/// <summary>
		/// 現在シーンの HUD / UI / Sprite / Font を、ポストエフェクト後の画面へ重ねて描画します。
		/// </summary>
		// Debug/ReleaseでHUD/UI/Sprite/Fontを必ず重ねるための共通ルート。
		void DrawCurrentScene2DOverlay();

		/// <summary>
		/// SceneRenderTarget へゲーム本編の3D描画結果を作成します。
		/// </summary>
		void DrawGameWorldToSceneTarget();

		/// <summary>
		/// SceneRenderTarget に対するポストエフェクト結果を BackBuffer へ出力します。
		/// </summary>
		void ApplyPostEffectToBackBuffer();

		/// <summary>
		/// BackBuffer へ戻したあと、ポストエフェクト対象外にしたい UI を直接描画します。
		/// </summary>
		void DrawGameUIToBackBuffer();

	private: /// ---------- 描画順序制御 ---------- ///

		// フレーム内の描画順序だけを集約する薄いController
		RenderPipelineController renderPipelineController_;

		// 現在シーンの更新・描画を管理するシーンマネージャー
		std::unique_ptr<SceneManager> sceneManager_;
	};


} // namespace Ken4lowEngine
