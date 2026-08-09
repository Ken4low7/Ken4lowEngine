#pragma once
#include "ActorComponent.h"
#include "Vector3.h"

#include <vector>
#include <json.hpp>

namespace Ken4lowEngine
{
	class Actor;

	/// -------------------------------------------------------------
	///		位置・回転・スケールと親子関係を持つComponentクラス
	/// -------------------------------------------------------------
	class SceneComponent : public ActorComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SceneComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// SceneComponentの1フレーム更新処理。
		/// </summary>
		void Update(float deltaTime) override;

		/// <summary>
		/// Editor停止中に親子Transformだけを再計算する。
		/// </summary>
		void UpdateEditor(float deltaTime) override;

		/// <summary>
		/// 現在のLocalTransformからWorldTransformを即座に再計算する。
		/// </summary>
		void RefreshWorldTransform()
		{
			UpdateWorldTransform(); // 物理補正後のLocalTransformを同フレーム中にWorldTransformへ反映する
		}

		/// <summary>
		/// SceneComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// Actorが所有するComponentを階層表示する。
		/// </summary>
		void DrawComponentHierarchyImGui(Actor*& selectedActor, ActorComponent*& selectedComponent);

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		virtual std::string GetClassTypeName() const override
		{
			return "SceneComponent";
		}

		virtual void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 有効状態 ---------- ///

		bool IsActiveInHierarchy() const override;

	public: /// ---------- 親子関係 ---------- ///

		void AttachTo(SceneComponent* parent);
		void Detach();
		SceneComponent* GetParent() const { return parent_; }
		const std::vector<SceneComponent*>& GetChildren() const { return children_; }

	public: /// ---------- Transform Getter ---------- ///

		const Vector3& GetLocalPosition() const { return localPosition_; }
		const Vector3& GetLocalRotation() const { return localRotation_; }
		const Vector3& GetLocalScale() const { return localScale_; }
		const Vector3& GetWorldPosition() const { return worldPosition_; }
		const Vector3& GetWorldRotation() const { return worldRotation_; }
		const Vector3& GetWorldScale() const { return worldScale_; }

	public: /// ---------- Transform Setter ---------- ///

		void SetLocalPosition(const Vector3& position)
		{
			localPosition_ = position;
		}

		void SetLocalRotation(const Vector3& rotation)
		{
			localRotation_ = rotation;
		}

		void SetLocalScale(const Vector3& scale)
		{
			localScale_ = scale;
		}

	public: /// ---------- Mutable Access ---------- ///

		Vector3& LocalPosition() { return localPosition_; }
		Vector3& LocalRotation() { return localRotation_; }
		Vector3& LocalScale() { return localScale_; }

	private: /// ---------- 内部処理 ---------- ///

		void UpdateWorldTransform();
		void RemoveChild(SceneComponent* child);

	private: /// ---------- メンバ変数 ---------- ///

		SceneComponent* parent_ = nullptr;
		std::vector<SceneComponent*> children_;

		Vector3 localPosition_{ 0.0f, 0.0f, 0.0f };
		Vector3 localRotation_{ 0.0f, 0.0f, 0.0f };
		Vector3 localScale_{ 1.0f, 1.0f, 1.0f };

		Vector3 worldPosition_{ 0.0f, 0.0f, 0.0f };
		Vector3 worldRotation_{ 0.0f, 0.0f, 0.0f };
		Vector3 worldScale_{ 1.0f, 1.0f, 1.0f };
	};
} // namespace Ken4lowEngine
