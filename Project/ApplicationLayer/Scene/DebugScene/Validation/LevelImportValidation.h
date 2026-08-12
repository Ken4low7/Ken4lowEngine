#pragma once

namespace Ken4lowEngine
{
	class ActorWorld;
	class PhysicsWorld;
}

namespace K4E = ::Ken4lowEngine;

/// 旧FPSステージ向けImport検証は廃止し、削除済みStage JSON/ModelをDebugScene起動時に読込まない。
class LevelImportValidation
{
public:
	explicit LevelImportValidation(K4E::ActorWorld* = nullptr, K4E::PhysicsWorld* = nullptr) noexcept
	{
		// DebugSceneの既存メンバ初期化との互換性だけを維持し、旧Stage Importは実行しない。
	}

	void DrawImGui() const noexcept
	{
		// 廃止済み検証UIの互換呼び出しは何も描画しない。
	}
};
