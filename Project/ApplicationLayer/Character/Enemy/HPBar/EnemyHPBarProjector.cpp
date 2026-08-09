#include "EnemyHPBarProjector.h"

HpBarProjectResult ProjectWorldToScreen(
    const K4E::Vector3& worldPos,
    const K4E::Matrix4x4& viewMatrix,
    const K4E::Matrix4x4& projectionMatrix,
    float screenWidth,
    float screenHeight)
{
    HpBarProjectResult result{};

    // =========================================
    // 1. まず view 空間へ変換して前方判定を行う
    // =========================================
    // MakeLookAtMatrix の実装を見る限り、左手座標系で前方は +Z 方向です。
    // そのため view 空間で z が 0 以下なら、
    // カメラの後ろ or ほぼカメラ面上とみなして非表示にします。
    const K4E::Vector3 viewPos = K4E::Vector3::Transform(worldPos, viewMatrix);

    if (viewPos.z <= 0.01f)
    {
        result.inFront = false;
        result.inScreen = false;
        return result;
    }

    result.inFront = true;

    // =========================================
    // 2. view * projection * viewport 行列を作る
    // =========================================
    // あなたの Matrix4x4 は
    // Vector3::Transform(v, M) が「v に対して M を適用する」作りなので、
    // world -> view -> projection -> viewport の順になるように
    // 行列をこの順で掛けます。
    const K4E::Matrix4x4 viewportMatrix =
        K4E::Matrix4x4::MakeViewportMatrix(
            0.0f,
            0.0f,
            screenWidth,
            screenHeight,
            0.0f,
            1.0f);

    const K4E::Matrix4x4 viewProjectionMatrix =
        K4E::Matrix4x4::Multiply(viewMatrix, projectionMatrix);

    const K4E::Matrix4x4 viewProjectionViewportMatrix =
        K4E::Matrix4x4::Multiply(viewProjectionMatrix, viewportMatrix);

    // =========================================
    // 3. ワールド座標をそのまま画面座標へ変換
    // =========================================
    // Vector3::Transform は内部で w 除算までやってくれるため、
    // viewport まで含めた行列を通せば、そのままスクリーン座標になります。
    const K4E::Vector3 screenPos3D =
        K4E::Vector3::Transform(worldPos, viewProjectionViewportMatrix);

    result.screenPos = { screenPos3D.x, screenPos3D.y };

    // =========================================
    // 4. 画面内判定
    // =========================================
    result.inScreen =
        (result.screenPos.x >= 0.0f && result.screenPos.x <= screenWidth) &&
        (result.screenPos.y >= 0.0f && result.screenPos.y <= screenHeight);

    return result;
}