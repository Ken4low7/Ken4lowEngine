#pragma once
#include <Camera.h>

#include <memory>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class WinApp;
	class DirectXCommon;


	/// -------------------------------------------------------------
	///				　			ゲーム全体
	/// -------------------------------------------------------------
	class Framework
	{
	public: /// ---------- 実行処理 ---------- ///

		/// <summary>
		/// アプリケーションのメインループを実行します。<br/>
		/// ・Initialize() で基盤システムを初期化したあと、<br/>
		/// ・WinApp::ProcessMessage() を使ったゲームループを回し、<br/>
		/// 毎フレーム Update() → Draw() を呼び出し、<br/>
		/// ・ループ終了後に Finalize() を呼び出して終了処理を行います。<br/>
		/// 通常、エントリポイント（WinMain）から一度だけ呼び出されます。
		/// </summary>
		void Run();

	public: /// ---------- ライフサイクル（派生クラスで拡張） ---------- ///

		/// <summary>
		/// 仮想デストラクタ。<br/>
		/// Framework を継承したクラスをポリモーフィックに delete できるようにします。
		/// </summary>
		virtual ~Framework() = default;

		/// <summary>
		/// ゲーム全体の初期化処理。<br/>
		/// ・WinApp の生成とウィンドウ作成<br/>
		/// ・DirectXCommon / 各種マネージャの初期化<br/>
		/// ・デフォルトカメラの生成・設定<br/>
		/// などを行います。<br/>
		/// 派生クラスで追加の初期化を行いたい場合は、オーバーライドして
		/// 冒頭か末尾で Framework::Initialize() を呼び出す想定です。
		/// </summary>
		virtual void Initialize();

		/// <summary>
		/// 毎フレームの更新処理。<br/>
		/// ・Wireframe / Object3DCommon / ParticleManager / GpuParticleManager などの更新<br/>
		/// を行います。<br/>
		/// 派生クラス側でゲーム固有の更新処理を追加したい場合はオーバーライドします。
		/// </summary>
		virtual void Update();

		/// <summary>
		/// 毎フレームの描画処理。<br/>
		/// Framework 側では純粋仮想関数として宣言しており、<br/>
		/// 実際の描画手順（レンダーターゲットのクリア、シーン描画、ポストエフェクト、ImGui など）は
		/// 派生クラスで実装します。
		/// </summary>
		virtual void Draw() = 0;

		/// <summary>
		/// アプリケーション終了時の後始末処理。<br/>
		/// ・WinApp / DirectXCommon の Finalize<br/>
		/// ・各種マネージャの終了処理（ParticleManager など）<br/>
		/// を行います。<br/>
		/// 派生クラスで追加の解放処理が必要な場合はオーバーライドします。
		/// </summary>
		virtual void Finalize();

	protected: /// ---------- メンバ変数 ---------- ///

		// OS ウィンドウ、メッセージ処理、表示設定を管理するアプリケーション基盤。
		WinApp* winApp_ = nullptr;

		// DirectX12 のデバイス、コマンド、スワップチェーンなどをまとめて扱う共通基盤。
		DirectXCommon* dxCommon_ = nullptr;

		// エンジン起動時に用意する標準カメラ。CameraManager のメインカメラとして登録する。
		std::unique_ptr<Camera> defaultCamera_;
	};


} // namespace Ken4lowEngine
