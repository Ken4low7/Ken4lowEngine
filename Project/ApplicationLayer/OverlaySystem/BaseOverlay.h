#pragma once

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine { class SceneManager; }

/// -------------------------------------------------------------
///						オーバーレイクラス
/// -------------------------------------------------------------
class BaseOverlay
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~BaseOverlay() = default;

	virtual void Initialize() {}

	/// <summary>
	/// オーバーレイを開く処理を行います。
	/// </summary>
	/// <param name="sceneManager">操作対象のシーン管理オブジェクトへのポインタ。シーンの作成や状態管理を行う SceneManager を指定します。</param>
	virtual void Open(Ken4lowEngine::SceneManager* sceneManager) { sceneManager_ = sceneManager; }

	/// <summary>
	/// 更新処理（閉じたいタイミングでClose()を呼ぶ）
	/// </summary>
	virtual void Update() = 0;

	/// <summary>
	/// 2D描画処理(ボタン・テキスト・暗転など)
	/// </summary>
	virtual void Draw2D() = 0;

	/// <summary>
	/// ImGui描画処理
	/// </summary>
	virtual void DrawImGui() {}

	/// <summary>
	/// オブジェクトを閉じる仮想メソッド。内部フラグ close_ を true に設定します。
	/// </summary>
	/// <returns>閉じる操作の成否を示す bool 値。実装では内部フラグ close_ を true に設定します。</returns>
	virtual void Close() { close_ = true; }

	/// <summary>
	/// オブジェクトが閉じているかどうかを示すブール値を返すアクセサメソッド。
	/// </summary>
	/// <returns>close_ メンバの値。オブジェクトが閉じている場合は true、そうでない場合は false。</returns>
	bool IsClose() const { return close_; }

	/// <summary>
	/// ワールドを一時停止するかどうかを示す仮想関数です。
	/// </summary>
	/// <returns>ワールドを一時停止する場合は true を返します。</returns>
	virtual bool PausesWorld() const { return true; }

	/// <summary>
	/// オブジェクトがモーダルであるかを示す仮想メソッド。実装では常に true を返します。
	/// </summary>
	/// <returns>オブジェクトがモーダルであれば true。ここでは常に true を返します。</returns>
	virtual bool IsModal() const { return true; }

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	virtual bool IsGoTitle() const { return goTitle_; }

protected: /// ---------- メンバ変数 ---------- ///

	Ken4lowEngine::SceneManager* sceneManager_ = nullptr; // シーン管理オブジェクトへのポインタ
	bool close_ = false; // オーバーレイを閉じるかどうかのフラグ
	bool goTitle_ = false; // タイトルに戻るかどうかのフラグ
};

