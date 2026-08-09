#pragma once

#include "Vector3.h"

#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>
	/// ShadowMapの注視位置をどの基準で決めるかを表す設定値です。<br/>
	/// LightManagerから独立した型にしておくことで、将来のCSM/SpotShadow/PointShadowでも
	/// ライト管理本体へ依存せずにShadow設定だけを受け渡せるようにします。
	/// </summary>
	enum class ShadowFocusMode : uint32_t
	{
		Camera = 0,
		Player = 1,
		StageCenter = 2,
		Manual = 3,
	};

	/// <summary>
	/// ShadowMapの品質・範囲・デバッグ表示に関わる設定値をまとめた値オブジェクトです。<br/>
	/// GPU定数バッファのレイアウトではなく、ParameterManagerや将来のShadowSystemへ
	/// LightManager内部状態を安全に受け渡すためのCPU側契約として扱います。
	/// </summary>
	struct ShadowSettings
	{
		// ShadowのON/OFFと比較品質。既存Shaderの定数バッファではなくCPU側設定なので、値の意味だけをここへ集約する。
		bool enableShadow = true;
		float shadowBias = 0.0f;
		float normalBias = 0.025f;
		float shadowStrength = 0.6f;

		// 現在は共通ShadowMap解像度として扱う。将来CSMやPointShadowで分割する場合は、Cascade/Light別設定を別構造体に追加する。
		uint32_t shadowMapSize = 2048;

		// DebugView用の表示フラグ。ShadowMap生成やライト選択の結果を変えない、表示専用の設定として扱う。
		bool showShadowMapDebug = false;
		bool showShadowFactorDebug = false;

		// Shadowを投影するライトの選択。将来Spot/Point/CSMへ拡張しても、LightManagerの所有データ自体はここへ移さない。
		int32_t shadowCasterLightIndex = -1;
		ShadowFocusMode shadowFocusMode = ShadowFocusMode::Camera;
		Vector3 manualShadowFocusPosition = { 0.0f, 0.0f, 0.0f };

		// 現行の単一Directional Shadow用の投影範囲。CSM化する場合は、この値を上書き利用せずCascadeごとの分割設定を別途持つ。
		float directionalShadowDistance = 60.0f;
		float directionalShadowWidth = 35.0f;
		float directionalShadowHeight = 35.0f;
		float directionalShadowNearZ = 0.1f;
		float directionalShadowFarZ = 120.0f;
		float directionalShadowFocusOffset = 0.0f;

		// SpotShadowの射影Near値。
		float spotShadowNearZ = 0.1f;

		// Point Light Cube Shadowは6面共通のNear値を持ち、Far値には選択Point Lightのradiusを使う。
		float pointShadowNearZ = 0.1f;

		// CSMは既存単一Directional Shadowと切り替え可能にし、初期OFFで従来の見た目を維持する。
		bool enableCsm = false;
		float csmMaxDistance = 160.0f;
		float csmSplitLambda = 0.7f;
	};
}
