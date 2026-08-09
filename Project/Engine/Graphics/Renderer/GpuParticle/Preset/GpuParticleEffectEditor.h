#pragma once

#include "GpuParticleEffectDesc.h"

#include <string>

namespace Ken4lowEngine
{

	/// <summary>
	/// 新しいEmitter設定をImGuiで編集するための土台を描画します。
	/// 既存GpuParticleEmitterや描画パイプラインへ値を反映しないため、現行GPUパーティクルの挙動は変更しません。
	/// </summary>
	void DrawEmitterDescImGui(GpuParticleEmitterDesc& desc);

	/// <summary>
	/// Effect名、Emitter一覧、追加・削除、JSON保存・読み込みをまとめて編集するImGui UIです。
	/// DebugSceneは編集データとUI状態だけを所有し、詳細なImGui処理はこの関数へ委譲します。
	/// </summary>
	void DrawGpuParticleEffectEditor(
		GpuParticleEffectDesc& effect,
		int& selectedEmitterIndex,
		std::string& jsonPath,
		std::string& statusMessage,
		bool& lastOperationSucceeded);

} // namespace Ken4lowEngine
