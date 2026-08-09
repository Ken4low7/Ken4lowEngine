#pragma once

#include "Vector4.h"

#include <cstddef>

namespace Ken4lowEngine
{
	class Collider;
	class PhysicsWorld;
	class Rigidbody;
	struct Contact;

	/// -------------------------------------------------------------
	///                   PhysicsWorld Debug描画設定
	/// -------------------------------------------------------------
	struct PhysicsDebugDrawSettings
	{
		bool drawPhysicsDebug = false;
		bool drawColliders = true;
		bool drawContacts = true;
		bool drawContactNormals = true;
		bool drawVelocity = true;
		bool drawSleeping = true;
		bool drawEvents = true;
		std::size_t maxColliderDrawCount = 64;
		float normalLength = 1.5f;
		float velocityScale = 0.25f;
	};

	/// -------------------------------------------------------------
	/// PhysicsWorldの状態を可視化し、衝突や物理挙動の調査をしやすくするDebug描画クラス
	/// -------------------------------------------------------------
	class PhysicsDebugDraw
	{
	public:
		// PhysicsWorld内のCollider/Contact/Rigidbody/EventをWireframeで描画する。
		void Draw(const PhysicsWorld& physicsWorld);

		// PhysicsWorld内のDebug情報と描画設定をImGuiで表示する。
		void DrawImGui(const PhysicsWorld& physicsWorld);

		// 外部からDebug描画設定をまとめて差し替える。
		void SetSettings(const PhysicsDebugDrawSettings& settings) { settings_ = settings; }

		// Debug描画設定を取得し、DebugScene/GamePlayWorld側でON/OFFを調整できるようにする。
		PhysicsDebugDrawSettings& GetSettings() { return settings_; }
		const PhysicsDebugDrawSettings& GetSettings() const { return settings_; }

	private:
		// Colliderの形状とBodyTypeに応じた色を決める。
		Vector4 GetColliderColor(const Collider& collider) const;

		// Collider形状をWireframeで描画する。
		void DrawCollider(const Collider& collider, const Vector4& color) const;

		// Contact pointとnormalを描画する。
		void DrawContact(const Contact& contact) const;

		// Rigidbodyに紐づくColliderから速度ベクトルを描画する。
		void DrawVelocity(const PhysicsWorld& physicsWorld, const Rigidbody& rigidbody) const;

		// BodyTypeを文字列に変換する。
		const char* ToBodyTypeName(const Rigidbody* rigidbody) const;

		// PhysicsEventTypeを文字列に変換する。
		const char* ToEventTypeName(int eventType) const;

	private:
		PhysicsDebugDrawSettings settings_{};
	};
} // namespace Ken4lowEngine
