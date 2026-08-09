#pragma once
#include "ColliderComponent.h"

namespace Ken4lowEngine
{
	/// Characterで共有する当たり判定設定とActor Transformへの追従処理を提供する。
	class CharacterColliderComponent : public ColliderComponent
	{
	public:
		/// Character向けの扱いやすい既定形状を設定する。
		CharacterColliderComponent();

		/// Player移行用の既定値よりJSON・Editorで設定したHalfSizeを優先する。
		void SetHalfSize(const Vector3& halfSize);

		/// Collider生成後に所有CharacterActorへCollision/Overlap通知を接続する。
		void Initialize() override;

		/// CharacterのWorld Scaleを実Collider形状へ毎フレーム反映する。
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "CharacterColliderComponent"; }

	private:
		void SyncScaledShape();
	};
} // namespace Ken4lowEngine
