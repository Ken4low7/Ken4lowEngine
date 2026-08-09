#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_set>

#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "IDGenerator.h"

#include "OBB.h"
#include "AABB.h"
#include "Segment.h"
#include "Capsule.h"
#include "Sphere.h"

enum class EObjectChannel : uint32_t;
enum class ECollisionResponse : uint8_t;

namespace Ken4lowEngine
{
	class Collider;
	class Rigidbody;

	/// CollisionHit は将来のBlock/Overlapイベントへ渡す詳細情報の最小単位。
	struct CollisionHit
	{
		Collider* self = nullptr;
		Collider* other = nullptr;
		Vector3 point{};
		Vector3 normal{};
		float distance = 0.0f;
		ECollisionResponse response{};
	};

	/// Primitive Colliderが扱う単純形状の種別。見た目のMeshとは分離した衝突用形状として扱う。
	enum class ECollisionShapeType : uint8_t
	{
		None,
		Sphere,
		AABB,
		OBB,
		Capsule,
		Segment,
	};

	/// CollisionShapeInfo はCollider内の形状データを将来CollisionShapeへ分離するための中間置き場。
	struct CollisionShapeInfo
	{
		static constexpr float kDrawEpsilon = 0.001f;

		ECollisionShapeType shapeType = ECollisionShapeType::OBB;
		Vector3 colliderPosition = { 0.0f, 0.0f, 0.0f };
		Vector3 colliderHalfSize = { 0.0f, 0.0f, 0.0f };
		Vector3 orientation = { 0.0f, 0.0f, 0.0f };
		Vector4 debugColor = { 0.0f, 1.0f, 1.0f, 1.0f };
		bool useOBB = true;
		Vector3 obbBasis[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
		bool useOBBBasis = false;
		Segment segment{};
		bool useSegment = true;
		Sphere sphere{};
		bool useSphere = false;
		Capsule capsule{};
		bool useCapsule = false;
		bool drawCapsule = false;

		// AABBはcenter/halfSizeから派生生成し、既存OBB情報と重複保持しない。
		AABB BuildAABB() const
		{
			return AABB{ colliderPosition - colliderHalfSize, colliderPosition + colliderHalfSize };
		}

		// OBB生成をShapeInfo側へ寄せ、ShapeType dispatchからも同じPrimitiveを取り出せるようにする。
		OBB BuildOBB() const
		{
			OBB obb{};
			obb.center = colliderPosition;
			obb.size = colliderHalfSize;

			if (useOBBBasis)
			{
				obb.orientations[0] = obbBasis[0];
				obb.orientations[1] = obbBasis[1];
				obb.orientations[2] = obbBasis[2];
				return obb;
			}

			const Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(orientation);
			obb.orientations[0] = Vector3::Normalize({ rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] });
			obb.orientations[1] = Vector3::Normalize({ rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] });
			obb.orientations[2] = Vector3::Normalize({ rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] });
			return obb;
		}

		// 形状ごとの描画可否判定をShapeInfo側へ寄せ、Collider本体から形状条件を少しずつ分離する。
		bool HasDrawableOBB() const
		{
			return colliderHalfSize.x > kDrawEpsilon || colliderHalfSize.y > kDrawEpsilon || colliderHalfSize.z > kDrawEpsilon;
		}

		bool HasDrawableSegment() const
		{
			return Vector3::Length(segment.diff) > kDrawEpsilon;
		}

		bool HasDrawableCapsule() const
		{
			return useCapsule && drawCapsule && capsule.radius > kDrawEpsilon;
		}
	};

	/// CollisionFilterData はTypeIDやPreset適用情報を将来CollisionComponentへ移すための中間置き場。
	struct CollisionFilterData
	{
		static constexpr uint32_t kMaxCollisionChannels = 32u;
		static constexpr uint8_t kDefaultResponseId = 0u; // Application側のECollisionResponse::Ignoreと同じ値。

		uint32_t typeID = 0u;
		uint32_t objectChannelId = 0u;
		bool enabled = true;
		bool queryEnabled = true;
		bool physicsEnabled = true;
		bool trigger = false;
		bool requireOwner = false;
		bool ownerActive = true;
		bool ownerAlive = true;
		bool ownerVisible = true;
		bool hasResponseOverrides = false;
		uint32_t collisionLayer = 0u;
		std::string presetName{};
		std::array<uint8_t, kMaxCollisionChannels> responseIds{};

		// 既存TypeIDとObjectChannel相当IDを同期し、段階移行中の判定互換性を保つ。
		void SetTypeId(uint32_t newTypeId)
		{
			typeID = newTypeId;
			objectChannelId = newTypeId;
		}

		// Preset未適用ColliderはResponseMatrixへフォールバックするため、個別Responseを無効として初期化する。
		void ResetResponses(uint8_t responseId = kDefaultResponseId)
		{
			responseIds.fill(responseId);
			hasResponseOverrides = false;
		}

		// Preset適用時だけ個別Responseを有効にし、Collider単位のObjectChannel判定に使えるようにする。
		void SetResponse(uint32_t otherObjectChannelId, uint8_t responseId)
		{
			if (otherObjectChannelId >= kMaxCollisionChannels) return;
			responseIds[otherObjectChannelId] = responseId;
			hasResponseOverrides = true;
		}
	};

	/// CollisionEventState はEnter/Stay/Exit用の接触履歴を将来イベント管理へ分離するための中間置き場。
	struct CollisionEventState
	{
		std::unordered_set<uint32_t> currentCollisions;
		std::unordered_set<uint32_t> prevCollisions;

		// Enter/Stay/Exitの履歴更新をEventState側へ寄せ、イベント管理分離の入口にする。
		void BeginFrame()
		{
			prevCollisions.swap(currentCollisions);
			currentCollisions.clear();
		}

		void AddContact(uint32_t otherUniqueId)
		{
			currentCollisions.insert(otherUniqueId);
		}
	};

	/// -------------------------------------------------------------
	///                     当たり判定クラス
	/// -------------------------------------------------------------
	class Collider
	{
	public: /// ---------- 仮想関数 ---------- ///

		// コンストラクタ
		Collider() : serialNumber_(IDGenerator::Generate()) {} // シリアルナンバーを生成

		// 仮想デストラクタ
		virtual ~Collider() = default;

		// 旧来の衝突通知（互換用）
		virtual void OnCollision([[maybe_unused]] Collider* other) {}

		/*
		Collision Event naming policy:
		- OnCollisionEnter/Stay/Exit は当面Block/Overlap兼用の互換イベントとして残す。
		- 将来のBlock専用イベントは OnCollisionEnter/Stay/Exit を物理的に遮る接触として扱う。
		- 将来のOverlap専用イベントは OnOverlapEnter/Stay/Exit をすり抜け接触通知として追加する。
		*/
		// Enter/Stay/Exit（デフォルトは互換のため OnCollision を呼ぶ）
		virtual void OnCollisionEnter(Collider* other) { OnCollision(other); }
		virtual void OnCollisionStay(Collider* other) { OnCollision(other); }
		virtual void OnCollisionExit([[maybe_unused]] Collider* other) {}

		// 詳細Hit情報を受け取る将来用入口。現段階では既存Collider*イベントへ委譲して互換性を保つ。
		virtual void OnCollisionEnter([[maybe_unused]] const CollisionHit& hit) { OnCollisionEnter(hit.other); }
		virtual void OnCollisionStay([[maybe_unused]] const CollisionHit& hit) { OnCollisionStay(hit.other); }
		virtual void OnCollisionExit([[maybe_unused]] const CollisionHit& hit) { OnCollisionExit(hit.other); }

		// Trigger/Overlap専用イベント。既存処理との互換のため、未override時は旧Collisionイベントへ委譲する。
		virtual void OnOverlapBegin([[maybe_unused]] const CollisionHit& hit) { OnCollisionEnter(hit.other); }
		virtual void OnOverlapStay([[maybe_unused]] const CollisionHit& hit) { OnCollisionStay(hit.other); }
		virtual void OnOverlapEnd([[maybe_unused]] const CollisionHit& hit) { OnCollisionExit(hit.other); }

	public: /// ---------- 衝突状態管理（マネージャ側が使用） ---------- ///

		// フレーム開始時に呼ぶ：prev <- current, current をクリア
		void BeginCollisionFrame();

		// このフレームで接触した相手IDを登録
		void AddCollisionThisFrame(uint32_t otherUniqueId);

		const std::unordered_set<uint32_t>& GetCurrentCollisions() const { return eventState_.currentCollisions; }
		const std::unordered_set<uint32_t>& GetPrevCollisions() const { return eventState_.prevCollisions; }

	public: /// ---------- OBBのメンバ関数 ---------- ///

		// 中心座標取得・設定
		virtual Vector3 GetCenterPosition() const { return shapeInfo_.colliderPosition; }
		virtual void SetCenterPosition(const Vector3& pos) { shapeInfo_.colliderPosition = pos; }

		// 半サイズ取得・設定
		virtual Vector3 GetOBBHalfSize() const { return shapeInfo_.colliderHalfSize; }
		virtual void SetOBBHalfSize(const Vector3& halfSize) { shapeInfo_.colliderHalfSize = halfSize; shapeInfo_.shapeType = ECollisionShapeType::OBB; }

		// 回転（オイラー角）取得・設定
		virtual Vector3 GetOrientation() const { return shapeInfo_.orientation; }
		virtual void SetOrientation(const Vector3& rot) { shapeInfo_.orientation = rot; shapeInfo_.shapeType = ECollisionShapeType::OBB; }

		OBB GetOBB() const;
		AABB GetAABB() const { return shapeInfo_.BuildAABB(); }

		// AABBをcenter/halfSizeへ展開して保持し、既存OBB用データと共存させる。
		void SetAABB(const AABB& aabb)
		{
			shapeInfo_.colliderPosition = (aabb.min + aabb.max) * 0.5f;
			shapeInfo_.colliderHalfSize = (aabb.max - aabb.min) * 0.5f;
			shapeInfo_.shapeType = ECollisionShapeType::AABB;
		}

		void SetOBBBasis(const Vector3& axisX, const Vector3& axisY, const Vector3& axisZ);
		void ClearOBBBasis();
		bool HasOBBBasis() const { return shapeInfo_.useOBBBasis; }

	public: /// ---------- セグメントのメンバ関数 ---------- ///

		// セグメントを設定（衝突判定用）
		void SetSegment(const Segment& segment) { shapeInfo_.segment = segment; shapeInfo_.shapeType = ECollisionShapeType::Segment; }

		// セグメントを取得
		virtual Segment GetSegment() const { return shapeInfo_.segment; }

	public: /// ---------- Sphere のメンバ関数 ---------- ///

		// Sphereを設定（衝突判定用）
		void SetSphere(Sphere& spere) { shapeInfo_.sphere = spere; shapeInfo_.useSphere = true; shapeInfo_.shapeType = ECollisionShapeType::Sphere; }

		// Sphereを取得
		virtual Sphere GetSphere() const { return shapeInfo_.sphere; }

	public: /// ---------- Capsule のメンバ関数 ---------- ///

		// Capsule を設定
		virtual void SetCapsule(const Capsule& capsule) { shapeInfo_.capsule = capsule; shapeInfo_.useCapsule = true; shapeInfo_.shapeType = ECollisionShapeType::Capsule; }

		// Capsule を取得
		virtual Capsule GetCapsule() const { return shapeInfo_.capsule; }

		// 使用フラグ（テーブル判定用）
		bool HasCapsule() const { return shapeInfo_.useCapsule; }

		// デバッグ可視化フラグの設定
		void SetCapsuleVisible(bool v) { shapeInfo_.drawCapsule = v; }

		// デバッグ可視化フラグの取得
		bool IsCapsuleVisible() const { return shapeInfo_.drawCapsule; }

		// 現在の主Primitive形状種別を取得（既存判定には未使用）
		ECollisionShapeType GetShapeType() const { return shapeInfo_.shapeType; }

	public: /// ---------- Rigidbody連携 ---------- ///

		// Rigidbody参照を設定する。nullptrを許容し、既存Colliderは未設定のまま動作できる。
		void SetRigidbody(Rigidbody* rigidbody) { rigidbody_ = rigidbody; }

		// Rigidbody参照を取得する。未設定の場合はnullptrを返す。
		Rigidbody* GetRigidbody() const { return rigidbody_; }

	public: /// ---------- デバッグ用メンバ関数 ---------- ///

		// 初期化処理
		void Initialize();

		// 更新処理
		void Update();

		// 描画処理（OBBの可視化）
		void Draw();

		// Collision Debug Viewer用に、Managerが決めた色で形状と補助情報を描画する。
		void DrawDebug(const Vector4& color, bool drawBounds = true);

		// ImGui描画処理
		void DrawImGui();

	public: /// ---------- 設定 ---------- ///

		// 識別IDを取得
		uint32_t GetTypeID() const { return filterData_.typeID; }

		// 識別IDを設定
		void SetTypeID(uint32_t typeID) { filterData_.SetTypeId(typeID); }

		// ObjectChannel相当のIDを取得（現段階では既存TypeIDと同値）
		uint32_t GetObjectChannelId() const { return filterData_.objectChannelId; }

		// ObjectChannel相当のIDを設定。TypeID互換を維持したい通常経路ではSetTypeIDを使う。
		void SetObjectChannelId(uint32_t objectChannelId) { filterData_.objectChannelId = objectChannelId; }

		// UE風ObjectChannel API。TypeIDは変えず、Preset/Response判定用のChannelだけを更新する。
		void SetObjectChannel(::EObjectChannel objectChannel) { SetObjectChannelId(static_cast<uint32_t>(objectChannel)); }
		::EObjectChannel GetObjectChannel() const { return static_cast<::EObjectChannel>(filterData_.objectChannelId); }

		// PhysicsWorld用のCollisionLayerを設定する。既存TypeID/ObjectChannelとは独立して扱う。
		void SetCollisionLayer(uint32_t layer) { filterData_.collisionLayer = layer; }
		uint32_t GetCollisionLayer() const { return filterData_.collisionLayer; }

		// Preset適用名を記録（Response適用はCollisionPreset/CollisionPresetLibrary側で行う）
		void SetCollisionPreset(std::string_view presetName) { SetCollisionPresetName(presetName); }
		void SetCollisionPresetName(std::string_view presetName) { filterData_.presetName = std::string(presetName); }
		std::string_view GetCollisionPresetName() const { return filterData_.presetName; }

		// Collider全体の有効状態。falseならManagerの更新・判定・Trace対象から外れる。
		void SetEnabled(bool enabled) { filterData_.enabled = enabled; }
		bool IsEnabled() const { return filterData_.enabled; }

		// Query有効状態をPresetから適用し、Debug表示と問い合わせ判定の段階移行に使う。
		void SetQueryEnabled(bool enabled) { filterData_.queryEnabled = enabled; }
		bool IsQueryEnabled() const { return filterData_.queryEnabled; }

		// Physics有効状態をPresetから適用し、将来のBlock押し戻し対象判定に使う。
		void SetPhysicsEnabled(bool enabled) { filterData_.physicsEnabled = enabled; }
		bool IsPhysicsEnabled() const { return filterData_.physicsEnabled; }

		// Triggerは押し戻し対象ではなく、Overlap通知だけを行うColliderとして扱う予定地。
		void SetTrigger(bool trigger) { filterData_.trigger = trigger; }
		bool IsTrigger() const { return filterData_.trigger; }

		// Level由来Colliderの用途と名前を、Trigger通知側で安全に識別できる軽量メタデータとして保持する。
		void SetCollisionTag(std::string_view tag) { collisionTag_ = tag; }
		std::string_view GetCollisionTag() const { return collisionTag_; }
		void SetDebugName(std::string_view name) { debugName_ = name; }
		std::string_view GetDebugName() const { return debugName_; }

		// Owner状態による除外条件。既存World Collider互換のため、Owner必須は明示設定時だけ有効にする。
		void SetRequireOwner(bool required) { filterData_.requireOwner = required; }
		bool RequiresOwner() const { return filterData_.requireOwner; }
		void SetOwnerActive(bool active) { filterData_.ownerActive = active; }
		void SetOwnerAlive(bool alive) { filterData_.ownerAlive = alive; }
		void SetOwnerVisible(bool visible) { filterData_.ownerVisible = visible; }
		bool IsOwnerActive() const { return filterData_.ownerActive; }
		bool IsOwnerAlive() const { return filterData_.ownerAlive; }
		bool IsOwnerVisible() const { return filterData_.ownerVisible; }

		// CollisionManagerが判定対象にしてよいかを一箇所で判断する。
		bool IsCollisionEnabledForQuery() const
		{
			if (!filterData_.enabled || !filterData_.queryEnabled) return false;
			if (filterData_.requireOwner && !owner_) return false;
			return filterData_.ownerActive && filterData_.ownerAlive && filterData_.ownerVisible;
		}

		// 将来の押し戻し/物理応答用。TriggerはQueryのみのOverlapとして扱う。
		bool IsCollisionEnabledForPhysics() const
		{
			return IsCollisionEnabledForQuery() && filterData_.physicsEnabled && !filterData_.trigger;
		}

		// Collider個別のResponseを初期化する。Preset未適用状態へ戻す場合にも使える。
		void ResetCollisionResponses(uint8_t defaultResponseId = CollisionFilterData::kDefaultResponseId)
		{
			filterData_.ResetResponses(defaultResponseId);
		}

		// ObjectChannel相当IDに対するResponseを保持する。値はApplication側ECollisionResponseの数値と合わせる。
		void SetCollisionResponseId(uint32_t otherObjectChannelId, uint8_t responseId)
		{
			filterData_.SetResponse(otherObjectChannelId, responseId);
		}

		// Collider個別Responseを持つかを返す。falseならCollisionManagerの既存Matrixへフォールバックする。
		bool HasCollisionResponseOverrides() const { return filterData_.hasResponseOverrides; }

		// ObjectChannel相当IDに対するResponse値を返す。範囲外はIgnore相当として扱う。
		uint8_t GetCollisionResponseId(uint32_t otherObjectChannelId) const
		{
			if (otherObjectChannelId >= CollisionFilterData::kMaxCollisionChannels)
			{
				return CollisionFilterData::kDefaultResponseId;
			}
			return filterData_.responseIds[otherObjectChannelId];
		}

		// シリアルナンバーを取得
		uint32_t GetUniqueID() const { return serialNumber_; }

		// オーナーを設定・取得
		template<class T> void SetOwner(T* ptr)
		{
			owner_ = ptr;
			ownerDebugName_ = ptr ? typeid(T).name() : "";
		}
		template<class T> T* GetOwner() const { return static_cast<T*>(owner_); }
		std::string_view GetOwnerDebugName() const { return ownerDebugName_; }

	private: /// ---------- メンバ変数 ---------- ///

		// 衝突設定データ（TypeID/Preset情報）
		CollisionFilterData filterData_{};

		// オーナー（任意のオブジェクトを指せるようにvoidポインタで持つ）
		void* owner_ = nullptr;

		// Debug表示用のOwner型名。所有権は持たず、SetOwner<T>時だけ更新する。
		std::string ownerDebugName_{};

		// Triggerのゲーム用途とLevelオブジェクト名を、OwnerなしStatic Colliderでも参照できるよう保持する。
		std::string collisionTag_{};
		std::string debugName_{};

		// 物理応答用Rigidbody参照。所有権は持たず、未設定Colliderは従来通り静的扱いにできる。
		Rigidbody* rigidbody_ = nullptr;

	private: /// ---------- 衝突履歴（Enter/Stay/Exit 用） ---------- ///

		CollisionEventState eventState_{};

	private: /// ---------- OBBのメンバ変数 ---------- ///

		CollisionShapeInfo shapeInfo_{};

	protected: /// ---------- シリアルナンバー ---------- ///

		// シリアルナンバー
		uint32_t serialNumber_ = 0;
	};

} // namespace Ken4lowEngine
