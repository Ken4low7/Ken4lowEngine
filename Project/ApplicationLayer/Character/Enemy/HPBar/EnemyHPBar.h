#pragma once

#include <memory>
#include <algorithm>
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

namespace K4E = ::Ken4lowEngine;

// 1体分の敵HPバーを管理するクラス
class EnemyHPBar
{
public:
    // 初期化
    void Initialize();

    // 更新
    // screenPos : 画面上の中心座標
    // hpRate    : 0.0f ～ 1.0f
    // visible   : 表示するかどうか
    // deltaTime : フレーム時間
    // width     : バーの幅
    // height    : バーの高さ
    void Update(
        const K4E::Vector2& screenPos,
        float hpRate,
        bool visible,
        float deltaTime,
        float width = 72.0f,
        float height = 8.0f);

    // 描画
    void Draw();

    // 表示切り替え
    void SetVisible(bool visible);

    // 表示中か
    bool IsVisible() const { return visible_; }

private:
    // HP割合に応じた色を返す
    K4E::Vector4 GetHpColor(float hpRate) const;

    // 値を 0.0f ～ 1.0f に丸める
    float Clamp01(float value) const;

private:
    // 枠として使うスプライト
    std::unique_ptr<K4E::Sprite> frameSprite_;

    // 背景として使うスプライト
    std::unique_ptr<K4E::Sprite> backSprite_;

    // 遅れて減るダメージバー
    std::unique_ptr<K4E::Sprite> damageDelaySprite_;

    // 現在HP量を表すスプライト
    std::unique_ptr<K4E::Sprite> fillSprite_;

    // 減った分が拡大して消えるフラッシュ演出
    std::unique_ptr<K4E::Sprite> damageFlashSprite_;

    // 表示フラグ
    bool visible_ = false;

    // 現在HP
    float currentHpRate_ = 1.0f;

    // 遅れて減るバー用HP
    float delayedHpRate_ = 1.0f;

    // 前回フレームHP
    float prevHpRate_ = 1.0f;

    // 遅延バーの待機時間
    float delayWaitTimer_ = 0.0f;

    // 減少分フラッシュ演出タイマー
    float flashTimer_ = 0.0f;

    // フラッシュ演出の全体時間
    float flashDuration_ = 0.22f;

    // フラッシュ開始時のHP割合
    float flashStartRate_ = 1.0f;

    // フラッシュ終了時のHP割合
    float flashEndRate_ = 1.0f;
};