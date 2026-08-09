#pragma once
#include "Sprite.h"
#include <ISceneTransition.h>

#include <memory>
#include <string>
#include <vector>
#include <random>

#include <Vector4.h>
#include <Vector2.h>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///					　	フェード管理クラス
/// -------------------------------------------------------------
class FadeManager final : public Ken4lowEngine::ISceneTransition
{
private: /// ---------- 列挙型 ---------- ///

	// 状態列挙型
	enum class State
	{
		None,
		TileCover,   // タイルが閉じる（覆う）: 回転＋拡縮しながら設置
		Hold,        // 完全に覆われた状態（シーン切替タイミング）
		Crack,       // ひび割れアニメーション（覆ったまま上にヒビを出す）
		TileUncover  // ドロップ中（タイルが落ちてシーンが見える）
	};

private:
	struct Tile
	{
		std::unique_ptr<K4E::Sprite> base;  // ブロック
		std::unique_ptr<K4E::Sprite> crack; // ひび割れ（CrackAtlas）

		K4E::Vector2 targetCenter{}; // 完成位置（中心）
		K4E::Vector2 startCenter{};  // 出現開始位置（中心）

		K4E::Vector2 pos{};
		K4E::Vector2 vel{};

		float startRot = 0.0f;
		float rot = 0.0f;
		float rotVel = 0.0f;

		float startScale = 1.0f;
		float scale = 1.0f;

		float delayCover = 0.0f; // 設置の遅延（波状に置く）
		float delayCrack = 0.0f; // ヒビ開始の遅延（波状に割る）
		float delayDrop = 0.0f; // ドロップ開始の遅延（波状に落とす）

		bool placed = false;
		bool dead = false;

		// ドロップ開始後の「アイテム化」用
		bool dropStarted = false;
		float dropTime = 0.0f;
	};

	// ------------------------------
	// ひび割れ中の粉塵（スプライト・CPU更新）
	// ※GPUパーティクルは使わない
	// ------------------------------
	struct DustParticle
	{
		std::unique_ptr<K4E::Sprite> sprite;
		bool active = false;

		K4E::Vector2 pos{};
		K4E::Vector2 vel{};
		float rot = 0.0f;
		float rotVel = 0.0f;

		float age = 0.0f;
		float life = 0.6f;
		float size0 = 6.0f;
		float size1 = 2.0f;
		float alpha0 = 0.55f;
	};

public: /// ---------- メンバ関数 ---------- ///

	// 初期化
	void Initialize() override;

	// 更新処理
	void Update(float dt) override;

	// 2D描画（タイル）
	void Draw2DSprites() override;

	// ImGuiデバッグ
	void DrawImGui() override;

	// Details Inspectorと専用FadeManagerウィンドウで同じDebug UIを共有する。
	void DrawInspectorContent() override;

	// 破棄
	void Finalize() override;

public: /// ---------- 外部から操作（SceneManager等から呼べる） ---------- ///

	// タイルで覆う（フェードアウト開始）
	void StartCover() override;

	// ひび割れアニメを開始（シーン切替後に呼ぶ想定）
	void StartCrack() override;

	// ドロップ開始（ひび割れ完了後に呼ぶ/自動遷移でもOK）
	void StartDrop();

	// 覆いが完了しているか（シーン切替の合図に）
	bool IsFullyCovered() const override { return state_ == State::Hold || state_ == State::Crack; }

	// ひび割れが完了しているか（次に「ドロップ」へ進める合図に使える）
	bool IsCrackDone() const { return crackDone_; }

	// ドロップが完了しているか（全タイルが画面外に落ちた）
	bool IsDropDone() const { return dropDone_; }

	// フェードが動作中か
	bool IsBusy() const override { return state_ != State::None; }

private:
	// タイル再構築
	void RebuildTiles(int screenW, int screenH);

	// タイルカバー更新
	void UpdateCover(float dt);

	// ひび割れ更新
	void UpdateCrack(float dt);

	// ドロップ更新
	void UpdateDrop(float dt);

	// ------------------------------
	// 粉塵（ひび割れ中）
	// ------------------------------
	void InitDustPool();
	void ResetDust();
	void EmitDust(const K4E::Vector2& origin, const K4E::Vector2& baseVel);
	void UpdateDust(float dt);

	// 補助
	static float Clamp01(float v);
	static float Lerp(float a, float b, float t);
	static K4E::Vector2 Lerp(const K4E::Vector2& a, const K4E::Vector2& b, float t);
	static float EaseOutBack(float t);
	static float EaseOutCubic(float t);
	static float RandRange(std::mt19937& rng, float a, float b);

private:
	// 状態
	State state_ = State::None;
	float stateTime_ = 0.0f;

	// ひび割れ完了フラグ
	bool crackDone_ = false;
	bool dropDone_ = false;

	// 画面サイズ（Spriteが使うクライアントサイズ）
	int screenW_ = 0;
	int screenH_ = 0;

	// タイル設定
	K4E::Vector2 tileSize_ = { 128.0f, 128.0f };
	int tilesX_ = 0;
	int tilesY_ = 0;

	std::string tileTexturePath_ = "Stage/rock.dds";

	// ひび割れアトラス
	std::string crackAtlasPath_ = "Effects/CrackAtlas.dds";
	K4E::Vector2 crackFrameSizePx_ = { 128.0f, 128.0f };
	static constexpr int kCrackFrames_ = 10;

	// カバー演出パラメータ
	float coverTileAnimTime_ = 0.18f;   // 1タイルの設置アニメ時間
	float coverStaggerTotal_ = 0.30f;   // 置く順の遅延総量（大きいほど波っぽい）
	float coverSpawnYOffset_ = 520.0f;  // 出現開始Yオフセット（上から降ってくる）

	// ひび割れ演出パラメータ
	float crackTileAnimTime_ = 1.5f;   // 1タイルが stage0->9 になる時間
	float crackStaggerTotal_ = 0.5f;   // ヒビ開始の遅延総量

	// ドロップ演出パラメータ
	float dropStartDelay_ = 0.05f;     // ひび割れ完了後、落下を始めるまでの待ち
	float dropItemScale_ = 0.18f;      // ドロップ開始時に「小さいアイテム」に縮むスケール
	float dropShrinkTime_ = 0.08f;     // 1→dropItemScale_ へ縮む時間
	float dropCrackFadeTime_ = 0.06f;  // ドロップ開始時にヒビを消すフェード時間
	float dropStaggerTotal_ = 0.20f;   // タイルごとの落下開始遅延の総量
	float dropGravity_ = 2600.0f;      // 重力（y+が下なら正）
	float dropDamping_ = 0.985f;       // 速度減衰（空気抵抗っぽい）
	float dropKickOut_ = 420.0f;       // 外側への初速（中心→外へ）
	float dropKickRand_ = 120.0f;      // 初速のランダムばらつき
	float dropKickDownMin_ = 80.0f;    // 落ち始めy初速（最小）
	float dropKickDownMax_ = 260.0f;   // 落ち始めy初速（最大）
	float dropRotVelMin_ = -10.0f;     // 回転速度
	float dropRotVelMax_ = 10.0f;      // 回転速度
	float dropKillMargin_ = 360.0f;    // 画面外に出たら消すマージン

	// タイル群
	std::vector<Tile> tiles_;
	std::mt19937 rng_{ 20260202 };

	// 粉塵（スプライト・パーティクル）
	std::vector<DustParticle> dust_;
	int dustCursor_ = 0;           // リングバッファ用
	float dustSpawnAcc_ = 0.0f;    // 生成レート積算

	// 粉塵設定（必要ならImGuiに出して調整してOK）
	std::string dustTexturePath_ = "Effects/black.dds"; // 小さい点/粉のテクスチャがあれば差し替え推奨（無ければ stone を流用）
	int dustMax_ = 96;
	float dustRate_ = 220.0f;      // 1秒あたり生成数（ひび割れ中）
	float dustGravity_ = 2000.0f;  // +Yが下
	float dustDamping_ = 0.88f;    // 60fps基準の減衰
	float dustLifeMin_ = 0.35f;
	float dustLifeMax_ = 0.75f;
	float dustSizeMin_ = 32.0f;
	float dustSizeMax_ = 64.0f;
	float dustAlphaMin_ = 0.18f;
	float dustAlphaMax_ = 0.55f;
	float dustKickMin_ = 80.0f;
	float dustKickMax_ = 360.0f;
	float dustUpBias_ = 220.0f;    // 初速に「上方向」を足して採掘っぽく
	float dustJitter_ = 140.0f;    // ばらつき

#ifdef USE_IMGUI
	// ImGui用の編集バッファ
	char tileTexBuf_[256]{};
	char crackTexBuf_[256]{};
#endif
};
