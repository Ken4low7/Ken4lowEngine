#pragma once
#include "ActorComponent.h"
#include "Vector3.h"

#include <cstddef>
#include <cstdint>
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
			RefreshWorldTransformHierarchy(); // 物理補正後のLocalTransformを同フレーム中にWorldTransformへ反映する。
		}

		/// Dirtyな自身/子階層だけを再計算し、実際に再計算したComponent数を返す。
		std::size_t RefreshWorldTransformHierarchy();

		/// LocalTransformまたは親階層の変更をWorldTransform dirtyとして記録する。
		void MarkTransformDirty();

		[[nodiscard]] bool IsWorldTransformDirty() const { return worldTransformDirty_; }
		[[nodiscard]] bool IsTransformHierarchyDirty() const { return subtreeTransformDirty_; }
		[[nodiscard]] std::uint64_t GetWorldTransformRevision() const { return worldTransformRevision_; }

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
			if (localPosition_ == position) return;
			localPosition_ = position;
			MarkTransformDirty();
		}

		void SetLocalRotation(const Vector3& rotation)
		{
			if (localRotation_ == rotation) return;
			localRotation_ = rotation;
			MarkTransformDirty();
		}

		void SetLocalScale(const Vector3& scale)
		{
			if (localScale_ == scale) return;
			localScale_ = scale;
			MarkTransformDirty();
		}

	public: /// ---------- Mutable Access ---------- ///

		Vector3& LocalPosition()
		{
			MarkTransformDirty();
			return localPosition_; // 既存の参照APIでも変更前にdirty化して追跡漏れを防ぐ。
		}

		Vector3& LocalRotation()
		{
			MarkTransformDirty();
			return localRotation_;
		}

		Vector3& LocalScale()
		{
			MarkTransformDirty();
			return localScale_;
		}

	private: /// ---------- 内部処理 ---------- ///

		std::size_t UpdateWorldTransform();
		void MarkWorldTransformDirtyRecursive();
		void MarkSubtreeDirtyUpward();
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

		bool worldTransformDirty_ = true;
		bool subtreeTransformDirty_ = true;
		std::uint64_t worldTransformRevision_ = 0;
		std::uint64_t lastParentWorldTransformRevision_ = 0;
	};
} // namespace Ken4lowEngine
