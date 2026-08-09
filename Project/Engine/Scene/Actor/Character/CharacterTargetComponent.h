#pragma once

#include "SceneComponent.h"

namespace Ken4lowEngine
{
	/// 攻撃、照準、追跡が参照するCharacterの代表位置をTransform階層上で管理するComponent。
	class CharacterTargetComponent : public SceneComponent
	{
	public:
		/// JSON保存・復元で使用するComponentクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterTargetComponent"; }

		/// AIや照準処理が追跡する明示的なWorld位置を設定する。
		void SetTargetPosition(const Vector3& targetPosition)
		{
			explicitTargetPosition_ = targetPosition;
			hasExplicitTargetPosition_ = true;
		}

		/// 明示位置を解除し、Character自身のターゲットポイントへ戻す。
		void ClearTargetPosition() { hasExplicitTargetPosition_ = false; }

		/// AIや照準処理から設定された明示位置を使用中か返す。
		bool HasExplicitTargetPosition() const { return hasExplicitTargetPosition_; }

		/// 明示位置があればその座標を返し、未設定時は親子Transform反映後の座標を返す。
		Vector3 GetTargetPosition() const
		{
			return hasExplicitTargetPosition_ ? explicitTargetPosition_ : GetWorldPosition();
		}

	private:
		Vector3 explicitTargetPosition_{};
		bool hasExplicitTargetPosition_ = false;
	};
} // namespace Ken4lowEngine
