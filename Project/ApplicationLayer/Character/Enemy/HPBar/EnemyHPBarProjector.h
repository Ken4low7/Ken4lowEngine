#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4x4.h"

namespace K4E = ::Ken4lowEngine;

// ワールド座標を画面座標へ変換した結果
struct HpBarProjectResult
{
    // 画面座標
    K4E::Vector2 screenPos{ 0.0f, 0.0f };

    // カメラ前方にあるか
    bool inFront = false;

    // 画面内にあるか
    bool inScreen = false;
};

// ワールド座標をスクリーン座標へ変換する
HpBarProjectResult ProjectWorldToScreen(
    const K4E::Vector3& worldPos,
    const K4E::Matrix4x4& viewMatrix,
    const K4E::Matrix4x4& projectionMatrix,
    float screenWidth,
    float screenHeight);