#pragma once
#include "Matrix4x4.h"
#include "SceneComponent.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>

namespace Ken4lowEngine
{
	class Object3D;

	/// -------------------------------------------------------------
	///   3D空間上でカメラ方向を向く板ポリ表示Componentクラス
	/// -------------------------------------------------------------
	class BillboardComponent : public SceneComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~BillboardComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		void Initialize() override;
		void Update(float deltaTime) override;
		void PostPhysicsUpdate(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "BillboardComponent"; // BillboardComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetTexturePath() const { return texturePath_; }
		void SetTexturePath(const std::string& texturePath);

		const Vector2& GetSize() const { return size_; }
		void SetSize(const Vector2& size) { size_ = size; }

		const Vector4& GetColor() const { return color_; }
		void SetColor(const Vector4& color) { color_ = color; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

		bool IsLockYAxis() const { return lockYAxis_; }
		void SetLockYAxis(bool lockYAxis) { lockYAxis_ = lockYAxis; }

		float GetRotationOffset() const { return rotationOffset_; }
		void SetRotationOffset(float rotationOffset) { rotationOffset_ = rotationOffset; }

	private: /// ---------- 内部処理 ---------- ///

		void EnsureObject3D();
		void ApplyBillboardTransform();
		Matrix4x4 BuildBillboardWorldMatrix() const;

	private: /// ---------- メンバ変数 ---------- ///

		std::string texturePath_ = "Effects/white.dds";
		Vector2 size_ = { 1.0f, 1.0f };
		Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible_ = true;
		bool lockYAxis_ = false;
		float rotationOffset_ = 0.0f;

		std::unique_ptr<Object3D> object3D_;
		std::string loadedTexturePath_;
	};
}
