#define NOMINMAX
#include "FadeManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <DirectXCommon.h>
#include "GameViewportConstants.h"

namespace K4E = ::Ken4lowEngine;

#ifdef USE_IMGUI
#include <imgui.h>
#include <Editor/EditorWindowManager.h>
#endif // USE_IMGUI

// ------------------------------
// 補助
// ------------------------------
float FadeManager::Clamp01(float v)
{
	return std::max(0.0f, std::min(1.0f, v));
}

float FadeManager::Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

K4E::Vector2 FadeManager::Lerp(const K4E::Vector2& a, const K4E::Vector2& b, float t)
{
	return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) };
}

// ちょい「ドン！」と収束する設置感（Back ease）
float FadeManager::EaseOutBack(float t)
{
	t = Clamp01(t);
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

float FadeManager::EaseOutCubic(float t)
{
	t = Clamp01(t);
	float u = 1.0f - t;
	return 1.0f - (u * u * u);
}


float FadeManager::RandRange(std::mt19937& rng, float a, float b)
{
	std::uniform_real_distribution<float> dist(a, b);
	return dist(rng);
}

// ------------------------------
// 初期化
// ------------------------------
void FadeManager::Initialize()
{
	// Fade演出はWinAppサイズではなく固定内部解像度1920x1080のタイルで構築する。
	screenW_ = static_cast<int>(K4E::GameViewportConstants::Width);
	screenH_ = static_cast<int>(K4E::GameViewportConstants::Height);

	RebuildTiles(screenW_, screenH_);
	InitDustPool();

#ifdef USE_IMGUI
	strncpy_s(tileTexBuf_, sizeof(tileTexBuf_), tileTexturePath_.c_str(), _TRUNCATE);
	strncpy_s(crackTexBuf_, sizeof(crackTexBuf_), crackAtlasPath_.c_str(), _TRUNCATE);
#endif

	state_ = State::None;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// タイル再構築（画面全体を埋める）
// ------------------------------
void FadeManager::RebuildTiles(int screenW, int screenH)
{
	tiles_.clear();

	tilesX_ = (int)std::ceil(screenW / tileSize_.x);
	tilesY_ = (int)std::ceil(screenH / tileSize_.y);

	const int total = tilesX_ * tilesY_;
	if (total <= 0)
	{
		return;
	}

	tiles_.reserve((size_t)total);

	// -----------------------------------------
	// orderCover: 置く順（中心から外へ + ちょいランダム）
	// orderCrack: 割る順（別の揺らぎで波を作る）
	// ※重要：sort comparator の中で乱数を呼ばない（invalid comparator対策）
	// -----------------------------------------
	std::vector<int> orderCover;
	orderCover.reserve((size_t)total);
	for (int i = 0; i < total; ++i) orderCover.push_back(i);

	std::vector<int> orderCrack = orderCover;

	const K4E::Vector2 center = { screenW * 0.5f, screenH * 0.5f };

	std::vector<float> keyCover(total);
	std::vector<float> keyCrack(total);

	for (int idx = 0; idx < total; ++idx)
	{
		int x = idx % tilesX_;
		int y = idx / tilesX_;

		K4E::Vector2 c = { (x + 0.5f) * tileSize_.x, (y + 0.5f) * tileSize_.y };
		float dx = c.x - center.x;
		float dy = c.y - center.y;
		float d = std::sqrt(dx * dx + dy * dy);

		// 乱数はここで1回だけ確定（比較中には呼ばない）
		keyCover[idx] = d + RandRange(rng_, -40.0f, 40.0f);
		keyCrack[idx] = d + RandRange(rng_, -80.0f, 80.0f);
	}

	std::sort(orderCover.begin(), orderCover.end(), [&](int a, int b)
		{
			return keyCover[a] < keyCover[b];
		});

	std::sort(orderCrack.begin(), orderCrack.end(), [&](int a, int b)
		{
			return keyCrack[a] < keyCrack[b];
		});

	// idx -> crackRank 変換（delayCrack を割り当てるため）
	std::vector<int> crackRank(total, 0);
	for (int rank = 0; rank < total; ++rank)
	{
		crackRank[orderCrack[rank]] = rank;
	}

	// delay を 0..Total に割り当て
	const int N = std::max(1, total);

	for (int rank = 0; rank < total; ++rank)
	{
		int idx = orderCover[rank];
		int x = idx % tilesX_;
		int y = idx / tilesX_;

		Tile t{};

		// -----------------------------------------
		// base
		// -----------------------------------------
		t.base = std::make_unique<K4E::Sprite>();
		t.base->Initialize(tileTexturePath_);
		t.base->SetAnchorPoint({ 0.5f, 0.5f }); // 回転中心

		// -----------------------------------------
		// crack
		// -----------------------------------------
		t.crack = std::make_unique<K4E::Sprite>();
		t.crack->Initialize(crackAtlasPath_);
		t.crack->SetAnchorPoint({ 0.5f, 0.5f });
		t.crack->SetColor({ 1,1,1,0 }); // 初期は非表示

		// 完成位置（中心）
		t.targetCenter = { (x + 0.5f) * tileSize_.x, (y + 0.5f) * tileSize_.y };

		// 出現開始：上から落ちてくる + ちょい横ブレ
		t.startCenter = t.targetCenter;
		t.startCenter.x += RandRange(rng_, -160.0f, 160.0f);
		t.startCenter.y -= coverSpawnYOffset_ + RandRange(rng_, 0.0f, 260.0f);

		t.pos = t.startCenter;

		// 初期回転とスケール（設置感）
		t.startRot = RandRange(rng_, -3.14159f, 3.14159f);
		t.rot = t.startRot;

		t.startScale = RandRange(rng_, 0.15f, 0.35f);
		t.scale = t.startScale;

		// 設置の遅延（0..coverStaggerTotal_）
		{
			float u = (N <= 1) ? 0.0f : (float)rank / (float)(N - 1);
			t.delayCover = u * coverStaggerTotal_;
		}

		// ひび割れの遅延（0..crackStaggerTotal_）
		{
			int r = crackRank[idx];
			float u = (N <= 1) ? 0.0f : (float)r / (float)(N - 1);
			t.delayCrack = u * crackStaggerTotal_;
		}

		// ドロップの遅延（0..dropStaggerTotal_） ※ヒビの順と揃えると気持ちいい
		{
			int r = crackRank[idx];
			float u = (N <= 1) ? 0.0f : (float)r / (float)(N - 1);
			t.delayDrop = u * dropStaggerTotal_;
		}

		t.placed = false;
		t.dead = false;

		// 初期反映（base）
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// 初期反映（crack）
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_); // stage0
		t.crack->Update();

		tiles_.push_back(std::move(t));
	}

	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// 開始：カバー
// ------------------------------
void FadeManager::StartCover()
{
	// 前の演出の粉塵を消す
	ResetDust();

	// Main Viewportの拡縮ではFade内部座標を変えない。
	int w = static_cast<int>(K4E::GameViewportConstants::Width);
	int h = static_cast<int>(K4E::GameViewportConstants::Height);

	if (w != screenW_ || h != screenH_)
	{
		screenW_ = w;
		screenH_ = h;
		RebuildTiles(screenW_, screenH_);
	}

	// 位置などを初期化（再スタート用）
	for (auto& t : tiles_)
	{
		t.pos = t.startCenter;
		t.rot = t.startRot;
		t.scale = t.startScale;
		t.placed = false;
		t.dead = false;

		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 }); // カバー開始時はヒビ無し
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_);
		t.crack->Update();
	}

	ResetDust();

	state_ = State::TileCover;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// 開始：ひび割れ（覆った後に呼ぶ）
// ------------------------------
void FadeManager::StartCrack()
{
	// まだ何もないなら無視
	if (tiles_.empty())
	{
		return;
	}

	// 覆っていない状態で呼ばれても動くようにはしておく（デバッグ用途）
	// 本運用では「Cover完了→シーン切替→StartCrack」を想定
	for (auto& t : tiles_)
	{
		// crackを stage0 に戻して透明に
		t.crack->SetUVRect({ 0.0f, 0.0f }, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->Update();
	}

	// 粉塵もここでリセット（ひび割れ開始と同時に出す）
	ResetDust();
	dustSpawnAcc_ = 0.0f;

	// 開始バースト：dt が跳ねた場合でも「粉が出た」と分かるように、最初に少しだけ撒く
	{
		const int kBurst = 48;
		const K4E::Vector2 center = { screenW_ * 0.5f, screenH_ * 0.5f };

		for (int i = 0; i < kBurst; ++i)
		{
			const Tile& tile = tiles_[(size_t)(RandRange(rng_, 0.0f, (float)tiles_.size() - 0.0001f))];

			K4E::Vector2 p = tile.pos;
			p.x += RandRange(rng_, -tileSize_.x * 0.45f, tileSize_.x * 0.45f);
			p.y += RandRange(rng_, -tileSize_.y * 0.45f, tileSize_.y * 0.45f);

			K4E::Vector2 dir = { p.x - center.x, p.y - center.y };
			float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (len < 1e-4f) { dir = { 1,0 }; len = 1.0f; }
			dir.x /= len;
			dir.y /= len;

			float kick = RandRange(rng_, dustKickMin_, dustKickMax_);
			K4E::Vector2 v{};
			v.x = dir.x * kick + RandRange(rng_, -dustJitter_, dustJitter_);
			v.y = dir.y * kick - dustUpBias_ + RandRange(rng_, -dustJitter_, dustJitter_);

			EmitDust(p, v);
		}
	}

	state_ = State::Crack;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}


void FadeManager::StartDrop()
{
	// まだ何もないなら無視
	if (tiles_.empty())
	{
		return;
	}

	// 画面中心から外へ飛ばしつつ、重力で落ちる
	const K4E::Vector2 center = { screenW_ * 0.5f, screenH_ * 0.5f };

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// 落下開始位置は「完成位置」に固定（覆い終わった状態から落とす）
		t.pos = t.targetCenter;
		t.scale = 1.0f;
		t.dropStarted = false;
		t.dropTime = 0.0f;

		// 外側への方向
		K4E::Vector2 dir = { t.targetCenter.x - center.x, t.targetCenter.y - center.y };
		float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
		if (len < 0.001f)
		{
			// 中心付近は適当に散らす
			dir = { RandRange(rng_, -1.0f, 1.0f), RandRange(rng_, -1.0f, 1.0f) };
			len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (len < 0.001f) len = 1.0f;
		}
		dir.x /= len;
		dir.y /= len;

		// 初速（外へ + 下へ）
		float kick = dropKickOut_ + RandRange(rng_, -dropKickRand_, dropKickRand_);
		t.vel.x = dir.x * kick + RandRange(rng_, -80.0f, 80.0f);
		t.vel.y = RandRange(rng_, dropKickDownMin_, dropKickDownMax_);

		// 回転
		t.rotVel = RandRange(rng_, dropRotVelMin_, dropRotVelMax_);

		// ひびは見える状態にしておく（割れてる感を保ったまま落下）
		// ※既にCrackが終わっていればalpha=1になっているはずだが、念のため
		// stageは最後(9)に固定
		K4E::Vector2 lt = { crackFrameSizePx_.x * (float)(kCrackFrames_ - 1), 0.0f };
		t.crack->SetUVRect(lt, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,1 });

		// 描画更新
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->Update();
	}

	// 粉塵は落下開始直後もしばらく残した方が「採掘中」の雰囲気が出るので残す。
	// （消したい場合は次の1行を有効化）
	// ResetDust();

	state_ = State::TileUncover; // Drop状態として使う
	stateTime_ = 0.0f;
	dropDone_ = false;
}

// ------------------------------
// 更新
// ------------------------------
void FadeManager::Update(float dt)
{
	// 何もしてないなら何もしない
	if (state_ == State::None)
	{
		return;
	}

	// 重い初期化などで dt が跳ねると、Crack/粉塵が一瞬で終わって「出てない」ように見えるので
// フェード演出だけ dt をクランプする（ゲーム全体の dt は触らない）
	const float kFadeDtMax = 1.0f / 30.0f; // 30fps 相当
	if (dt > kFadeDtMax) { dt = kFadeDtMax; }

	// 画面サイズの変化に追従（ウィンドウリサイズ対応）
	{
		// Main Viewportの拡縮ではFade内部座標を変えない。
		int w = static_cast<int>(K4E::GameViewportConstants::Width);
		int h = static_cast<int>(K4E::GameViewportConstants::Height);
		if (w != screenW_ || h != screenH_)
		{
			screenW_ = w;
			screenH_ = h;
			RebuildTiles(screenW_, screenH_);

			// 変化直後はカバー中扱いにしておく
			state_ = State::TileCover;
			stateTime_ = 0.0f;
			crackDone_ = false;
		}
	}

	stateTime_ += dt;

	switch (state_)
	{
	case State::TileCover:
		UpdateCover(dt);
		break;

	case State::Hold:
		// 完全に覆われた状態
		// ここで SceneManager がシーンを切り替え、切り替え後に StartCrack() を呼ぶ想定
		break;

	case State::Crack:
		UpdateCrack(dt);
		break;

	case State::TileUncover:
		UpdateDrop(dt);
		break;

	default:
		break;
	}

	// 粉塵の物理更新（Crack中に生成され、状態が変わっても消えるまで更新したい場合はここで回す）
	UpdateDust(dt);
}

// ------------------------------
// タイル設置（回転＋拡縮）
// ------------------------------
void FadeManager::UpdateCover(float dt)
{
	(void)dt;

	bool allPlaced = true;

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// delay後に設置開始
		float local = (stateTime_ - t.delayCover) / std::max(0.0001f, coverTileAnimTime_);
		float u = Clamp01(local);

		float e = EaseOutBack(u);

		t.pos = Lerp(t.startCenter, t.targetCenter, e);
		t.rot = Lerp(t.startRot, 0.0f, e);
		t.scale = Lerp(t.startScale, 1.0f, e);

		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// crackは非表示のまま位置合わせ
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,0 });
		t.crack->Update();

		if (u >= 0.999f)
		{
			t.placed = true;
		}
		else
		{
			allPlaced = false;
		}
	}

	// 全タイル設置完了 → Holdへ
	if (allPlaced)
	{
		state_ = State::Hold;
		stateTime_ = 0.0f;
	}
}

// ------------------------------
// ひび割れアニメーション
// ------------------------------
void FadeManager::UpdateCrack(float dt)
{
	// ひび割れ中に粉塵を出す
	std::vector<int> cracking;
	cracking.reserve(tiles_.size());

	bool allDone = true;

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// delay後にヒビ開始
		float local = (stateTime_ - t.delayCrack) / std::max(0.0001f, crackTileAnimTime_);

		// まだ開始前：非表示のまま
		if (local < 0.0f)
		{
			allDone = false;
			continue;
		}

		float u = Clamp01(local);

		// stage 0..9
		int stage = (int)(u * (float)kCrackFrames_);
		if (stage < 0) stage = 0;
		if (stage > (kCrackFrames_ - 1)) stage = (kCrackFrames_ - 1);

		// alpha：最初だけ少しフェードインさせる
		float a = Clamp01(u * 1.25f); // すぐ見えるように
		if (u >= 1.0f) a = 1.0f;

		K4E::Vector2 lt = { crackFrameSizePx_.x * (float)stage, 0.0f };

		t.crack->SetUVRect(lt, crackFrameSizePx_);
		t.crack->SetColor({ 1,1,1,a });
		t.crack->Update();

		// 「今まさに割れているタイル」を候補として集める（uが0..1の間）
		if (u > 0.001f && u < 0.999f)
		{
			int idx = (int)(&t - tiles_.data());
			cracking.push_back(idx);
		}

		if (u < 0.999f)
		{
			allDone = false;
		}
	}

	// ----- 粉塵エミット（生成数をレート制御して、タイル数が多くても破綻しないようにする） -----
	if (!cracking.empty())
	{
		// 生成数 = rate * dt の積算
		dustSpawnAcc_ += dt * dustRate_;
		int spawn = (int)dustSpawnAcc_;
		dustSpawnAcc_ -= (float)spawn;

		const K4E::Vector2 center = { screenW_ * 0.5f, screenH_ * 0.5f };

		for (int i = 0; i < spawn; ++i)
		{
			int pick = cracking[(size_t)(RandRange(rng_, 0.0f, (float)cracking.size() - 0.0001f))];
			const Tile& tile = tiles_[(size_t)pick];

			// タイル内のランダム位置から出す（中心±45%）
			K4E::Vector2 p = tile.pos;
			p.x += RandRange(rng_, -tileSize_.x * 0.45f, tileSize_.x * 0.45f);
			p.y += RandRange(rng_, -tileSize_.y * 0.45f, tileSize_.y * 0.45f);

			// 画面中心→外側に飛ぶ成分 + 上方向（-Y）
			K4E::Vector2 dir = { p.x - center.x, p.y - center.y };
			float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (len < 1e-4f) { dir = { 1,0 }; len = 1.0f; }
			dir.x /= len;
			dir.y /= len;

			float kick = RandRange(rng_, dustKickMin_, dustKickMax_);
			K4E::Vector2 v{};
			v.x = dir.x * kick + RandRange(rng_, -dustJitter_, dustJitter_);
			v.y = dir.y * kick - dustUpBias_ + RandRange(rng_, -dustJitter_, dustJitter_);

			EmitDust(p, v);
		}
	}

	crackDone_ = allDone;

	// 割れ終わったら、そのままドロップへ
	if (allDone)
	{
		StartDrop();
	}
}


// ------------------------------
// ドロップ更新（重力で落下）
// ------------------------------
void FadeManager::UpdateDrop(float dt)
{
	bool allDead = true;

	// 速度減衰をdtに合わせて（60fps基準）
	float damp = std::pow(dropDamping_, dt * 60.0f);

	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }

		// ドロップの開始待ち（全体の待ち + タイルごとの遅延）
		float local = stateTime_ - dropStartDelay_ - t.delayDrop;
		if (local < 0.0f)
		{
			// まだ落ちないタイルがあるので終わってない
			allDead = false;
			continue;
		}

		// ドロップ開始した瞬間に「アイテム化（縮む）」を開始
		if (!t.dropStarted)
		{
			t.dropStarted = true;
			t.dropTime = 0.0f;
		}

		t.dropTime += dt;

		// 1) スケールを 1.0 -> dropItemScale_ に素早く縮める（アイテム化）
		float shrinkT = Clamp01(t.dropTime / std::max(0.0001f, dropShrinkTime_));
		float se = EaseOutCubic(shrinkT);
		t.scale = Lerp(1.0f, dropItemScale_, se);

		// 2) ひび割れはドロップ開始と同時に消して「アイテムが落ちる」っぽくする
		float crackT = Clamp01(t.dropTime / std::max(0.0001f, dropCrackFadeTime_));
		float crackA = 1.0f - crackT;

		// 物理（y+が下想定）
		t.vel.y += dropGravity_ * dt;
		t.vel.x *= damp;
		t.vel.y *= damp;

		t.pos.x += t.vel.x * dt;
		t.pos.y += t.vel.y * dt;

		t.rot += t.rotVel * dt;

		// 画面外に出たら消す（中心基準なのでスケールを考慮）
		float halfH = (tileSize_.y * t.scale) * 0.5f;
		if (t.pos.y - halfH > (float)screenH_ + dropKillMargin_)
		{
			t.dead = true;

			// 念のため透明化
			t.base->SetColor({ 1,1,1,0 });
			t.base->Update();
			t.crack->SetColor({ 1,1,1,0 });
			t.crack->Update();
			continue;
		}

		// 更新（base）※縮んだ小さいアイテムとして描く
		t.base->SetPosition(t.pos);
		t.base->SetRotation(t.rot);
		t.base->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.base->SetColor({ 1,1,1,1 });
		t.base->Update();

		// 更新（crack）※フェードで消す
		t.crack->SetPosition(t.pos);
		t.crack->SetRotation(t.rot);
		t.crack->SetSize({ tileSize_.x * t.scale, tileSize_.y * t.scale });
		t.crack->SetColor({ 1,1,1,crackA });
		t.crack->Update();

		allDead = false;
	}

	if (allDead)
	{
		dropDone_ = true;
		state_ = State::None;
		stateTime_ = 0.0f;
	}
}

// ------------------------------
// 描画
// ------------------------------
void FadeManager::Draw2DSprites()
{
	if (state_ == State::None)
	{
		return;
	}

	// base（ブロック）
	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }
		t.base->Draw();
	}

	// crack（Crack中または割れ完了後も描く。alpha=0のタイルは何も見えない）
	for (auto& t : tiles_)
	{
		if (t.dead) { continue; }
		t.crack->Draw();
	}

	// dust（粉塵）
	for (auto& p : dust_)
	{
		if (!p.active) { continue; }
		p.sprite->Draw();
	}
}

// ------------------------------
// ImGui
// ------------------------------
void FadeManager::DrawInspectorContent()
{
#ifdef USE_IMGUI
	// Detailsと専用FadeManagerウィンドウの表示差分をなくすため、内部状態UIを共有する。
	const char* st =
		(state_ == State::None) ? "演出していない待機状態" :
		(state_ == State::TileCover) ? "タイルが配置されて画面を覆う段階" :
		(state_ == State::Hold) ? "待機" :
		(state_ == State::Crack) ? "ひび割れアニメーション中" :
		(state_ == State::TileUncover) ? "タイルを解除して画面が戻る段階" : "Unknown";

	ImGui::Text("Current State: %s", st);
	ImGui::Text("Fade Busy: %s", IsBusy() ? "Yes" : "No");
	ImGui::Text("Timer: %.3f", stateTime_);
	ImGui::Text("Cover Duration: %.3f / Crack Duration: %.3f / Drop Delay: %.3f", coverTileAnimTime_, crackTileAnimTime_, dropStartDelay_);
	ImGui::Text("ひび割れ: %s", crackDone_ ? "完了" : "途中");
	ImGui::Text("落下演出: %s", dropDone_ ? "落下完了" : "落下中");

	ImGui::Separator();

	if (ImGui::Button("Start Cover (Place Tiles)"))
	{
		StartCover();
	}
	ImGui::SameLine();
	if (ImGui::Button("Start Crack K4E::Animation"))
	{
		StartCrack();
	}
	ImGui::SameLine();
	if (ImGui::Button("Start Drop"))
	{
		StartDrop();
	}

	ImGui::SeparatorText("Tile Settings");

	bool rebuild = false;
	rebuild |= ImGui::SliderFloat2("Tile Size", &tileSize_.x, 32.0f, 256.0f);

	ImGui::SeparatorText("Cover K4E::Params");
	ImGui::SliderFloat("Cover Tile Anim Time", &coverTileAnimTime_, 0.05f, 0.6f);
	ImGui::SliderFloat("Cover Stagger Total", &coverStaggerTotal_, 0.0f, 1.0f);
	ImGui::SliderFloat("Spawn Y Offset", &coverSpawnYOffset_, 0.0f, 1400.0f);

	ImGui::SeparatorText("Crack K4E::Params");
	ImGui::SliderFloat("Crack Tile Anim Time", &crackTileAnimTime_, 0.05f, 2.0f);
	ImGui::SliderFloat("Crack Stagger Total", &crackStaggerTotal_, 0.0f, 1.5f);

	ImGui::SeparatorText("Dust (CPU K4E::Sprite Particle)");
	ImGui::SliderFloat("Dust Rate", &dustRate_, 0.0f, 1200.0f);
	ImGui::SliderFloat("Dust Gravity", &dustGravity_, 0.0f, 8000.0f);
	ImGui::SliderFloat("Dust Damping", &dustDamping_, 0.60f, 0.99f);
	ImGui::SliderFloat("Dust Life Min", &dustLifeMin_, 0.05f, 2.0f);
	ImGui::SliderFloat("Dust Life Max", &dustLifeMax_, 0.05f, 2.0f);
	ImGui::SliderFloat("Dust Size Min", &dustSizeMin_, 1.0f, 32.0f);
	ImGui::SliderFloat("Dust Size Max", &dustSizeMax_, 1.0f, 64.0f);
	ImGui::SliderFloat("Dust Alpha Min", &dustAlphaMin_, 0.0f, 1.0f);
	ImGui::SliderFloat("Dust Alpha Max", &dustAlphaMax_, 0.0f, 1.0f);
	ImGui::SliderFloat("Dust Kick Min", &dustKickMin_, 0.0f, 1200.0f);
	ImGui::SliderFloat("Dust Kick Max", &dustKickMax_, 0.0f, 1800.0f);
	ImGui::SliderFloat("Dust Up Bias", &dustUpBias_, 0.0f, 1200.0f);
	ImGui::SliderFloat("Dust Jitter", &dustJitter_, 0.0f, 800.0f);

	ImGui::SeparatorText("Drop K4E::Params");
	ImGui::SliderFloat("Drop Start Delay", &dropStartDelay_, 0.0f, 1.0f);
	ImGui::SliderFloat("Drop Item Scale", &dropItemScale_, 0.02f, 0.60f);
	ImGui::SliderFloat("Drop Shrink Time", &dropShrinkTime_, 0.01f, 0.40f);
	ImGui::SliderFloat("Drop Crack Fade Time", &dropCrackFadeTime_, 0.01f, 0.40f);
	ImGui::SliderFloat("Drop Stagger Total", &dropStaggerTotal_, 0.0f, 1.5f);
	ImGui::SliderFloat("Drop Gravity", &dropGravity_, 0.0f, 8000.0f);
	ImGui::SliderFloat("Drop Damping", &dropDamping_, 0.90f, 0.999f);
	ImGui::SliderFloat("Drop Kick Out", &dropKickOut_, 0.0f, 2000.0f);
	ImGui::SliderFloat("Drop Kick Rand", &dropKickRand_, 0.0f, 800.0f);
	ImGui::SliderFloat("Drop Kick Down Min", &dropKickDownMin_, -2000.0f, 2000.0f);
	ImGui::SliderFloat("Drop Kick Down Max", &dropKickDownMax_, -2000.0f, 2000.0f);
	ImGui::SliderFloat("Drop RotVel Min", &dropRotVelMin_, -40.0f, 0.0f);
	ImGui::SliderFloat("Drop RotVel Max", &dropRotVelMax_, 0.0f, 40.0f);
	ImGui::SliderFloat("Drop Kill Margin", &dropKillMargin_, 0.0f, 1200.0f);

	ImGui::SeparatorText("Textures");

	ImGui::InputText("Tile Texture", tileTexBuf_, sizeof(tileTexBuf_));
	ImGui::InputText("Crack Atlas", crackTexBuf_, sizeof(crackTexBuf_));

	if (ImGui::Button("Apply Texture Paths"))
	{
		tileTexturePath_ = tileTexBuf_;
		crackAtlasPath_ = crackTexBuf_;
		rebuild = true;
	}

	rebuild |= ImGui::SliderFloat2("Crack Frame(px)", &crackFrameSizePx_.x, 8.0f, 512.0f);

	if (ImGui::Button("Rebuild Tiles Now"))
	{
		rebuild = true;
	}

	if (rebuild)
	{
		// Debug再構築時も固定内部解像度1920x1080でタイルを作り直す。
		screenW_ = static_cast<int>(K4E::GameViewportConstants::Width);
		screenH_ = static_cast<int>(K4E::GameViewportConstants::Height);
		RebuildTiles(screenW_, screenH_);
	}

#else
	// ImGui無効なら何もしない
#endif
}

void FadeManager::DrawImGui()
{
#ifdef USE_IMGUI
	// WindowメニューのFadeManager表示フラグを×ボタン状態と共有する
	auto& editorWindowState = K4E::EditorWindowManager::GetInstance()->GetWindowState();
	if (!editorWindowState.showFadeManager)
	{
		return;
	}

	if (ImGui::Begin("FadeManager", &editorWindowState.showFadeManager))
	{
		DrawInspectorContent();
	}
	ImGui::End();
#else
	// ImGui無効なら何もしない
#endif
}

// ------------------------------
// 終了
// ------------------------------
void FadeManager::Finalize()
{
	tiles_.clear();
	dust_.clear();
	state_ = State::None;
	stateTime_ = 0.0f;
	crackDone_ = false;
	dropDone_ = false;
}

// ------------------------------
// 粉塵（CPUスプライト・パーティクル）
// ------------------------------
void FadeManager::InitDustPool()
{
	// 既に作ってあれば何もしない（ウィンドウリサイズなどでInitializeが呼ばれ直す可能性に備える）
	if (!dust_.empty()) { return; }

	dust_.resize((size_t)dustMax_);
	for (auto& p : dust_)
	{
		p.sprite = std::make_unique<K4E::Sprite>();
		p.sprite->Initialize(dustTexturePath_);
		p.sprite->SetAnchorPoint({ 0.5f, 0.5f });
		p.sprite->SetColor({ 1,1,1,0 });
		p.sprite->Update();
		p.active = false;
	}

	dustCursor_ = 0;
	dustSpawnAcc_ = 0.0f;
}

void FadeManager::ResetDust()
{
	for (auto& p : dust_)
	{
		p.active = false;
		if (p.sprite)
		{
			p.sprite->SetColor({ 1,1,1,0 });
			p.sprite->Update();
		}
	}
	dustCursor_ = 0;
	dustSpawnAcc_ = 0.0f;
}

void FadeManager::EmitDust(const K4E::Vector2& origin, const K4E::Vector2& baseVel)
{
	if (dust_.empty()) { return; }

	DustParticle& p = dust_[(size_t)dustCursor_];
	dustCursor_ = (dustCursor_ + 1) % (int)dust_.size();

	p.active = true;
	p.pos = origin;
	p.vel = baseVel;
	p.rot = RandRange(rng_, -3.14159f, 3.14159f);
	p.rotVel = RandRange(rng_, -10.0f, 10.0f);
	p.age = 0.0f;
	p.life = RandRange(rng_, dustLifeMin_, dustLifeMax_);
	p.size0 = RandRange(rng_, dustSizeMin_, dustSizeMax_);
	p.size1 = std::max(1.0f, p.size0 * RandRange(rng_, 0.25f, 0.55f));
	p.alpha0 = RandRange(rng_, dustAlphaMin_, dustAlphaMax_);

	// 見た目：石粉っぽく少しグレー
	K4E::Vector4 col{ 0.85f, 0.85f, 0.85f, p.alpha0 };

	p.sprite->SetPosition(p.pos);
	p.sprite->SetRotation(p.rot);
	p.sprite->SetSize({ p.size0, p.size0 });
	p.sprite->SetColor(col);
	p.sprite->Update();
}

void FadeManager::UpdateDust(float dt)
{
	if (dust_.empty()) { return; }

	// 速度減衰をdtに合わせて（60fps基準）
	float damp = std::pow(dustDamping_, dt * 60.0f);

	for (auto& p : dust_)
	{
		if (!p.active) { continue; }

		p.age += dt;
		if (p.age >= p.life)
		{
			p.active = false;
			p.sprite->SetColor({ 1,1,1,0 });
			p.sprite->Update();
			continue;
		}

		// 物理（+Yが下）
		p.vel.y += dustGravity_ * dt;
		p.vel.x *= damp;
		p.vel.y *= damp;
		p.pos.x += p.vel.x * dt;
		p.pos.y += p.vel.y * dt;
		p.rot += p.rotVel * dt;

		float t = Clamp01(p.age / std::max(0.0001f, p.life));

		// サイズは縮ませる（粉が散って小さくなる感じ）
		float s = Lerp(p.size0, p.size1, t);

		// 後半でフェードアウト
		float fadeStart = 0.55f;
		float a = p.alpha0;
		if (t >= fadeStart)
		{
			float u = (t - fadeStart) / std::max(0.0001f, 1.0f - fadeStart);
			a = p.alpha0 * (1.0f - Clamp01(u));
		}

		p.sprite->SetPosition(p.pos);
		p.sprite->SetRotation(p.rot);
		p.sprite->SetSize({ s, s });
		p.sprite->SetColor({ 0.85f, 0.85f, 0.85f, a });
		p.sprite->Update();
	}
}
