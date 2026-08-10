from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def path(rel: str) -> Path:
    return ROOT / rel


def read(rel: str) -> str:
    return path(rel).read_text(encoding="utf-8-sig")


def write(rel: str, text: str) -> None:
    p = path(rel)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8", newline="\n")


def replace_once(rel: str, old: str, new: str) -> None:
    text = read(rel)
    if new in text:
        return
    if old not in text:
        raise RuntimeError(f"replace target not found: {rel}\n{old[:180]}")
    write(rel, text.replace(old, new, 1))


def replace_regex_once(rel: str, pattern: str, replacement: str) -> None:
    text = read(rel)
    new_text, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count == 0:
        if replacement in text:
            return
        raise RuntimeError(f"regex target not found: {rel}\n{pattern[:180]}")
    write(rel, new_text)


def remove_lines_containing(rel: str, needles: list[str]) -> None:
    text = read(rel)
    lines = text.splitlines(keepends=True)
    lines = [line for line in lines if not any(needle in line for needle in needles)]
    write(rel, "".join(lines))


def apply_ids_and_handles() -> None:
    write(
        "Project/Engine/Scene/Actor/Core/WorldObjectId.h",
        r'''#pragma once

#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>ActorWorld内でActorを識別する実行時ID。0は無効値。</summary>
	struct ActorId
	{
		uint64_t value = 0;

		constexpr bool IsValid() const { return value != 0; }
		constexpr explicit operator bool() const { return IsValid(); }
		constexpr bool operator==(const ActorId&) const = default;
	};

	/// <summary>ActorWorld内でComponentを識別する実行時ID。0は無効値。</summary>
	struct ComponentId
	{
		uint64_t value = 0;

		constexpr bool IsValid() const { return value != 0; }
		constexpr explicit operator bool() const { return IsValid(); }
		constexpr bool operator==(const ComponentId&) const = default;
	};
} // namespace Ken4lowEngine
''')
    write(
        "Project/Engine/Scene/Actor/Core/ActorHandle.h",
        r'''#pragma once

#include "WorldObjectId.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Actorの生ポインタを長期間保持せず、ActorIdからActorWorldへ解決するための軽量Handle。
	/// Worldを所有しないため、World破棄後にdangling pointerを残さない。
	/// </summary>
	class ActorHandle
	{
	public:
		constexpr ActorHandle() = default;
		explicit constexpr ActorHandle(ActorId id) : id_(id) {}

		constexpr ActorId GetId() const { return id_; }
		constexpr bool IsSet() const { return id_.IsValid(); }
		constexpr void Reset() { id_ = {}; }
		constexpr explicit operator bool() const { return IsSet(); }
		constexpr bool operator==(const ActorHandle&) const = default;

	private:
		ActorId id_{};
	};
} // namespace Ken4lowEngine
''')

    ac = "Project/Engine/Scene/Actor/Core/ActorComponent.h"
    replace_once(ac, "#pragma once\n\n#include <cstdint>", "#pragma once\n\n#include \"WorldObjectId.h\"\n\n#include <cstdint>")
    replace_once(ac, "\tclass Actor;\n", "\tclass Actor;\n\tclass ActorWorld;\n")
    replace_once(
        ac,
        "\t\tvoid InitializeForWorld()\n\t\t{\n\t\t\tif (isInitialized_) return;\n\t\t\tInitialize();\n\t\t\tisInitialized_ = true;\n\t\t}\n",
        "\t\tvoid InitializeForWorld()\n\t\t{\n\t\t\tif (isInitialized_) return;\n\t\t\tInitialize();\n\t\t\tisInitialized_ = true;\n\t\t\tNotifyOwnerRuntimeStateChanged(); // 初期化で生成されたPhysics実体をWorldへ通知する。\n\t\t}\n")
    replace_once(
        ac,
        "\t\tvoid SetActive(bool active)\n\t\t{\n\t\t\tisActive_ = active; // Editor上で一時的にComponentの更新・描画を止めるためのフラグ\n\t\t}\n",
        "\t\tvoid SetActive(bool active)\n\t\t{\n\t\t\tif (isActive_ == active) return;\n\t\t\tisActive_ = active; // Editor上で一時的にComponentの更新・描画を止めるためのフラグ\n\t\t\tNotifyOwnerRuntimeStateChanged(); // Physics登録などWorld側の外部状態をイベントで同期する。\n\t\t}\n")
    replace_once(
        ac,
        "\t\tvoid SetUpdateOrder(int order)\n\t\t{\n\t\t\tupdateOrder_ = order; // Updateの実行順を設定する\n\t\t}\n",
        "\t\tvoid SetUpdateOrder(int order)\n\t\t{\n\t\t\tif (updateOrder_ == order) return;\n\t\t\tupdateOrder_ = order; // Updateの実行順を設定する\n\t\t\tNotifyOwnerOrderChanged(true, false);\n\t\t}\n")
    replace_once(
        ac,
        "\t\tvoid SetDrawOrder(int order)\n\t\t{\n\t\t\tdrawOrder_ = order; // Drawの実行順を設定する\n\t\t}\n",
        "\t\tvoid SetDrawOrder(int order)\n\t\t{\n\t\t\tif (drawOrder_ == order) return;\n\t\t\tdrawOrder_ = order; // Drawの実行順を設定する\n\t\t\tNotifyOwnerOrderChanged(false, true);\n\t\t}\n")
    replace_once(
        ac,
        "\tpublic: /// ---------- 名前設定 ---------- ///\n",
        "\tpublic: /// ---------- Runtime ID ---------- ///\n\n\t\tComponentId GetId() const { return id_; }\n\n\tpublic: /// ---------- 名前設定 ---------- ///\n")
    replace_once(
        ac,
        "\tprotected: /// ---------- メンバ変数 ---------- ///\n\n\t\t// 所有権はActor側にあり、ActorComponent側ではdeleteしない\n",
        "\tprivate: /// ---------- ActorWorld連携 ---------- ///\n\n\t\tfriend class ActorWorld;\n\t\tvoid SetId(ComponentId id) { id_ = id; }\n\t\tvoid NotifyOwnerRuntimeStateChanged();\n\t\tvoid NotifyOwnerOrderChanged(bool updateOrderChanged, bool drawOrderChanged);\n\n\tprotected: /// ---------- メンバ変数 ---------- ///\n\n\t\tComponentId id_{}; // JSONへ保存しないWorld実行時ID。\n\n\t\t// 所有権はActor側にあり、ActorComponent側ではdeleteしない\n")

    ah = "Project/Engine/Scene/Actor/Core/Actor.h"
    replace_once(ah, "#include \"SceneComponent.h\"\n", "#include \"SceneComponent.h\"\n#include \"WorldObjectId.h\"\n")
    replace_once(ah, "namespace Ken4lowEngine\n{\n", "namespace Ken4lowEngine\n{\n\tclass ActorWorld;\n")
    replace_once(
        ah,
        "\t\t\tcomponents_.push_back(std::move(component));\n\t\t\treturn ref;\n",
        "\t\t\tcomponents_.push_back(std::move(component));\n\t\t\tNotifyComponentAdded(ref);\n\t\t\treturn ref;\n")
    replace_once(
        ah,
        "\tpublic: /// ---------- 名前設定 ---------- ///\n",
        "\tpublic: /// ---------- Runtime ID ---------- ///\n\n\t\tActorId GetId() const { return id_; }\n\n\tpublic: /// ---------- 名前設定 ---------- ///\n")
    replace_once(
        ah,
        "\t\tvoid SetActive(bool active)\n\t\t{\n\t\t\tisActive_ = active; // Actor単位でUpdate/Draw対象に含めるかを切り替える\n\t\t}\n",
        "\t\tvoid SetActive(bool active)\n\t\t{\n\t\t\tif (isActive_ == active) return;\n\t\t\tisActive_ = active; // Actor単位でUpdate/Draw対象に含めるかを切り替える\n\t\t\tNotifyActorRuntimeStateChanged();\n\t\t}\n")
    replace_once(
        ah,
        "\t\tconst ActorComponent* FindComponentByName(std::string_view name) const;\n",
        "\t\tconst ActorComponent* FindComponentByName(std::string_view name) const;\n\n\t\tActorComponent* FindComponentById(ComponentId id);\n\t\tconst ActorComponent* FindComponentById(ComponentId id) const;\n")
    replace_once(
        ah,
        "\tprivate: /// ---------- メンバ変数 ---------- ///\n\n\t\t// Actor全体の基準Transform。所有権はcomponents_側が持つ\n",
        "\tprivate: /// ---------- ActorWorld連携 ---------- ///\n\n\t\tfriend class ActorWorld;\n\t\tfriend class ActorComponent;\n\t\tvoid SetId(ActorId id) { id_ = id; }\n\t\tvoid SetWorld(ActorWorld* world) { world_ = world; }\n\t\tvoid NotifyComponentAdded(ActorComponent& component);\n\t\tvoid NotifyComponentRemoving(ActorComponent& component);\n\t\tvoid NotifyComponentRuntimeStateChanged(ActorComponent& component);\n\t\tvoid NotifyActorRuntimeStateChanged();\n\t\tvoid MarkComponentOrderDirty(bool updateOrderChanged, bool drawOrderChanged);\n\n\tprivate: /// ---------- メンバ変数 ---------- ///\n\n\t\tActorId id_{}; // World内だけで有効な実行時ID。\n\t\tActorWorld* world_ = nullptr; // 所有権はActorWorld側。\n\t\tbool updateOrderCacheDirty_ = true;\n\t\tbool drawOrderCacheDirty_ = true;\n\n\t\t// Actor全体の基準Transform。所有権はcomponents_側が持つ\n")

    awh = "Project/Engine/Scene/Actor/Core/ActorWorld.h"
    replace_once(awh, "#include \"Actor.h\"\n", "#include \"Actor.h\"\n#include \"ActorHandle.h\"\n")
    replace_once(awh, "#include <vector>\n", "#include <vector>\n#include <unordered_map>\n")
    replace_once(
        awh,
        "\t\tActor* FindActorByName(std::string_view name, bool includeInactive = true);\n",
        "\t\tActor* FindActorByName(std::string_view name, bool includeInactive = true);\n\n\t\tActor* FindActorById(ActorId id);\n\t\tconst Actor* FindActorById(ActorId id) const;\n\t\tActorComponent* FindComponentById(ComponentId id);\n\t\tconst ActorComponent* FindComponentById(ComponentId id) const;\n\t\tActorHandle MakeActorHandle(const Actor* actor) const;\n\t\tActor* ResolveActor(const ActorHandle& handle);\n\t\tconst Actor* ResolveActor(const ActorHandle& handle) const;\n\t\tbool IsActorHandleValid(const ActorHandle& handle) const;\n")
    replace_once(
        awh,
        "\tprivate: /// ---------- 内部処理 ---------- ///\n",
        "\tprivate: /// ---------- 内部処理 ---------- ///\n\n\t\tfriend class Actor;\n\t\tvoid PrepareActorForWorld(Actor& actor);\n\t\tvoid ReleaseActorFromWorld(Actor& actor);\n\t\tvoid OnComponentAdded(Actor& actor, ActorComponent& component);\n\t\tvoid OnComponentRemoving(Actor& actor, ActorComponent& component);\n\t\tvoid OnComponentRuntimeStateChanged(Actor& actor, ActorComponent& component);\n\t\tvoid OnActorRuntimeStateChanged(Actor& actor);\n")
    replace_once(
        awh,
        "\t\t// ActorWorldがActorの寿命を管理する\n\t\tstd::vector<std::unique_ptr<Actor>> actors_;\n",
        "\t\t// ActorWorldがActorの寿命を管理する\n\t\tstd::vector<std::unique_ptr<Actor>> actors_;\n\n\t\tstd::unordered_map<uint64_t, Actor*> actorsById_;\n\t\tstd::unordered_map<uint64_t, ActorComponent*> componentsById_;\n\t\tuint64_t nextActorId_ = 1;\n\t\tuint64_t nextComponentId_ = 1;\n")

    acpp = "Project/Engine/Scene/Actor/Core/Actor.cpp"
    replace_once(acpp, "#include \"Actor.h\"\n", "#include \"Actor.h\"\n#include \"ActorWorld.h\"\n")
    replace_once(
        acpp,
        "namespace Ken4lowEngine\n{\n\tvoid Actor::Initialize()",
        "namespace Ken4lowEngine\n{\n\tvoid ActorComponent::NotifyOwnerRuntimeStateChanged()\n\t{\n\t\tif (owner_) owner_->NotifyComponentRuntimeStateChanged(*this);\n\t}\n\n\tvoid ActorComponent::NotifyOwnerOrderChanged(bool updateOrderChanged, bool drawOrderChanged)\n\t{\n\t\tif (owner_) owner_->MarkComponentOrderDirty(updateOrderChanged, drawOrderChanged);\n\t}\n\n\tvoid Actor::NotifyComponentAdded(ActorComponent& component)\n\t{\n\t\tMarkComponentOrderDirty(true, true);\n\t\tif (world_) world_->OnComponentAdded(*this, component);\n\t}\n\n\tvoid Actor::NotifyComponentRemoving(ActorComponent& component)\n\t{\n\t\tMarkComponentOrderDirty(true, true);\n\t\tif (world_) world_->OnComponentRemoving(*this, component);\n\t}\n\n\tvoid Actor::NotifyComponentRuntimeStateChanged(ActorComponent& component)\n\t{\n\t\tif (world_) world_->OnComponentRuntimeStateChanged(*this, component);\n\t}\n\n\tvoid Actor::NotifyActorRuntimeStateChanged()\n\t{\n\t\tif (world_) world_->OnActorRuntimeStateChanged(*this);\n\t}\n\n\tvoid Actor::MarkComponentOrderDirty(bool updateOrderChanged, bool drawOrderChanged)\n\t{\n\t\tif (updateOrderChanged) updateOrderCacheDirty_ = true;\n\t\tif (drawOrderChanged) drawOrderCacheDirty_ = true;\n\t}\n\n\tvoid Actor::Initialize()")
    replace_once(
        acpp,
        "\t\tfor (auto& component : components_)\n\t\t{\n\t\t\tif (!component) continue;\n\t\t\tcomponent->FinalizeForWorld();\n\t\t\tcomponent->SetOwner(nullptr); // Component破棄時に所有Actorへの参照を残さない。\n\t\t}\n\n\t\tcomponents_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する\n",
        "\t\tfor (auto& component : components_)\n\t\t{\n\t\t\tif (!component) continue;\n\t\t\tNotifyComponentRemoving(*component);\n\t\t\tcomponent->FinalizeForWorld();\n\t\t\tcomponent->SetOwner(nullptr); // Component破棄時に所有Actorへの参照を残さない。\n\t\t}\n\n\t\tcomponents_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する\n\t\tMarkComponentOrderDirty(true, true);\n")
    replace_once(
        acpp,
        "\t\t// Finalize直後にunique_ptrを破棄し、Component内部の所有リソースはRAIIで解放する。\n\t\t(*removeIt)->FinalizeForWorld();\n\t\t(*removeIt)->SetOwner(nullptr);\n\t\tcomponents_.erase(removeIt);\n\t\treturn true;\n",
        "\t\t// World側のID/Physics参照を先に外してからComponent本体を破棄する。\n\t\tNotifyComponentRemoving(*(*removeIt));\n\t\t(*removeIt)->FinalizeForWorld();\n\t\t(*removeIt)->SetOwner(nullptr);\n\t\tcomponents_.erase(removeIt);\n\t\tMarkComponentOrderDirty(true, true);\n\t\tNotifyActorRuntimeStateChanged(); // Rigidbody削除時は残ったColliderの関連付けも再同期する。\n\t\treturn true;\n")
    replace_once(
        acpp,
        "\tvoid Actor::AddTag(const std::string& tag)\n",
        "\tActorComponent* Actor::FindComponentById(ComponentId id)\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tfor (auto& component : components_)\n\t\t{\n\t\t\tif (component && component->GetId() == id) return component.get();\n\t\t}\n\t\treturn nullptr;\n\t}\n\n\tconst ActorComponent* Actor::FindComponentById(ComponentId id) const\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tfor (const auto& component : components_)\n\t\t{\n\t\t\tif (component && component->GetId() == id) return component.get();\n\t\t}\n\t\treturn nullptr;\n\t}\n\n\tvoid Actor::AddTag(const std::string& tag)\n")

    awcpp = "Project/Engine/Scene/Actor/Core/ActorWorld.cpp"
    replace_once(
        awcpp,
        "\tActor* ActorWorld::FindActorByName(std::string_view name, bool includeInactive)\n",
        "\tActor* ActorWorld::FindActorById(ActorId id)\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tconst auto found = actorsById_.find(id.value);\n\t\treturn found != actorsById_.end() ? found->second : nullptr;\n\t}\n\n\tconst Actor* ActorWorld::FindActorById(ActorId id) const\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tconst auto found = actorsById_.find(id.value);\n\t\treturn found != actorsById_.end() ? found->second : nullptr;\n\t}\n\n\tActorComponent* ActorWorld::FindComponentById(ComponentId id)\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tconst auto found = componentsById_.find(id.value);\n\t\treturn found != componentsById_.end() ? found->second : nullptr;\n\t}\n\n\tconst ActorComponent* ActorWorld::FindComponentById(ComponentId id) const\n\t{\n\t\tif (!id.IsValid()) return nullptr;\n\t\tconst auto found = componentsById_.find(id.value);\n\t\treturn found != componentsById_.end() ? found->second : nullptr;\n\t}\n\n\tActorHandle ActorWorld::MakeActorHandle(const Actor* actor) const\n\t{\n\t\treturn actor && OwnsActor(actor) ? ActorHandle(actor->GetId()) : ActorHandle{};\n\t}\n\n\tActor* ActorWorld::ResolveActor(const ActorHandle& handle)\n\t{\n\t\treturn FindActorById(handle.GetId());\n\t}\n\n\tconst Actor* ActorWorld::ResolveActor(const ActorHandle& handle) const\n\t{\n\t\treturn FindActorById(handle.GetId());\n\t}\n\n\tbool ActorWorld::IsActorHandleValid(const ActorHandle& handle) const\n\t{\n\t\tconst Actor* actor = ResolveActor(handle);\n\t\treturn actor && !actor->IsPendingDestroy();\n\t}\n\n\tActor* ActorWorld::FindActorByName(std::string_view name, bool includeInactive)\n")
    replace_once(
        awcpp,
        "\tActor* ActorWorld::AddActorToWorld(std::unique_ptr<Actor> actor, bool initializeActor)\n\t{\n\t\tif (!actor) return nullptr;\n\t\tActor* spawnedActor = actor.get();\n\t\tspawnedActor->SetName(MakeUniqueActorName(spawnedActor->GetName())); // 全生成経路でActor名の一意性を保証する。\n",
        "\tActor* ActorWorld::AddActorToWorld(std::unique_ptr<Actor> actor, bool initializeActor)\n\t{\n\t\tif (!actor) return nullptr;\n\t\tActor* spawnedActor = actor.get();\n\t\tspawnedActor->SetName(MakeUniqueActorName(spawnedActor->GetName())); // 全生成経路でActor名の一意性を保証する。\n\t\tPrepareActorForWorld(*spawnedActor);\n")
    replace_once(
        awcpp,
        "\tvoid ActorWorld::ProcessPendingActorAdds()\n",
        "\tvoid ActorWorld::PrepareActorForWorld(Actor& actor)\n\t{\n\t\tActorId actorId = actor.GetId();\n\t\tif (!actorId.IsValid() || (actorsById_.contains(actorId.value) && actorsById_[actorId.value] != &actor))\n\t\t{\n\t\t\tactorId = ActorId{ nextActorId_++ };\n\t\t\tactor.SetId(actorId);\n\t\t}\n\t\tactorsById_[actorId.value] = &actor;\n\t\tactor.SetWorld(this);\n\t\tfor (const auto& component : actor.GetComponents())\n\t\t{\n\t\t\tif (component) OnComponentAdded(actor, *component);\n\t\t}\n\t}\n\n\tvoid ActorWorld::ReleaseActorFromWorld(Actor& actor)\n\t{\n\t\tfor (const auto& component : actor.GetComponents())\n\t\t{\n\t\t\tif (!component) continue;\n\t\t\tif (component->GetId().IsValid()) componentsById_.erase(component->GetId().value);\n\t\t\tcomponent->SetId({});\n\t\t}\n\t\tif (actor.GetId().IsValid()) actorsById_.erase(actor.GetId().value);\n\t\tactor.SetWorld(nullptr);\n\t\tactor.SetId({});\n\t}\n\n\tvoid ActorWorld::OnComponentAdded(Actor& actor, ActorComponent& component)\n\t{\n\t\tComponentId componentId = component.GetId();\n\t\tif (!componentId.IsValid() || (componentsById_.contains(componentId.value) && componentsById_[componentId.value] != &component))\n\t\t{\n\t\t\tcomponentId = ComponentId{ nextComponentId_++ };\n\t\t\tcomponent.SetId(componentId);\n\t\t}\n\t\tcomponentsById_[componentId.value] = &component;\n\t\tif (isInitialized_ && component.IsInitialized()) RegisterPhysicsComponents(actor);\n\t}\n\n\tvoid ActorWorld::OnComponentRemoving(Actor& actor, ActorComponent& component)\n\t{\n\t\tif (isInitialized_) UnregisterPhysicsComponents(actor);\n\t\tif (component.GetId().IsValid()) componentsById_.erase(component.GetId().value);\n\t\tcomponent.SetId({});\n\t}\n\n\tvoid ActorWorld::OnComponentRuntimeStateChanged(Actor& actor, ActorComponent&)\n\t{\n\t\tif (!isInitialized_) return;\n\t\tRegisterPhysicsComponents(actor);\n\t}\n\n\tvoid ActorWorld::OnActorRuntimeStateChanged(Actor& actor)\n\t{\n\t\tif (!isInitialized_) return;\n\t\tif (actor.IsActive() && !actor.IsPendingDestroy()) RegisterPhysicsComponents(actor);\n\t\telse UnregisterPhysicsComponents(actor);\n\t}\n\n\tvoid ActorWorld::ProcessPendingActorAdds()\n")
    replace_once(
        awcpp,
        "\t\t\t\tUnregisterPhysicsComponents(*actor);\n\t\t\t\tactor->FinalizeForWorld();\n\t\t\t\tlastActorJsonSaveMessage_ = \"Destroyed : \" + actorName;\n",
        "\t\t\t\tUnregisterPhysicsComponents(*actor);\n\t\t\t\tactor->FinalizeForWorld();\n\t\t\t\tReleaseActorFromWorld(*actor);\n\t\t\t\tlastActorJsonSaveMessage_ = \"Destroyed : \" + actorName;\n")
    replace_once(
        awcpp,
        "\t\t\t// Actor破棄前にComponent側のFinalizeまで流す\n\t\t\tactor->FinalizeForWorld();\n\t\t}\n\n\t\tactors_.clear();",
        "\t\t\t// Actor破棄前にComponent側のFinalizeまで流す\n\t\t\tactor->FinalizeForWorld();\n\t\t\tReleaseActorFromWorld(*actor);\n\t\t}\n\n\t\tactors_.clear();")
    replace_once(
        awcpp,
        "\t\tfor (PendingActorAdd& pending : pendingActorAdds_)\n\t\t{\n\t\t\tif (pending.actor) pending.actor->FinalizeForWorld();\n\t\t}\n\t\tpendingActorAdds_.clear();\n",
        "\t\tfor (PendingActorAdd& pending : pendingActorAdds_)\n\t\t{\n\t\t\tif (!pending.actor) continue;\n\t\t\tpending.actor->FinalizeForWorld();\n\t\t\tReleaseActorFromWorld(*pending.actor);\n\t\t}\n\t\tpendingActorAdds_.clear();\n\t\tactorsById_.clear();\n\t\tcomponentsById_.clear();\n")


def apply_order_cache() -> None:
    ah = "Project/Engine/Scene/Actor/Core/Actor.h"
    replace_once(
        ah,
        "\t\tvoid MarkComponentOrderDirty(bool updateOrderChanged, bool drawOrderChanged);\n",
        "\t\tvoid MarkComponentOrderDirty(bool updateOrderChanged, bool drawOrderChanged);\n\t\tconst std::vector<ActorComponent*>& GetUpdateOrderedComponents();\n\t\tconst std::vector<ActorComponent*>& GetDrawOrderedComponents();\n")
    replace_once(
        ah,
        "\t\tbool updateOrderCacheDirty_ = true;\n\t\tbool drawOrderCacheDirty_ = true;\n",
        "\t\tbool updateOrderCacheDirty_ = true;\n\t\tbool drawOrderCacheDirty_ = true;\n\t\tstd::vector<ActorComponent*> updateOrderedComponents_;\n\t\tstd::vector<ActorComponent*> drawOrderedComponents_;\n")

    acpp = "Project/Engine/Scene/Actor/Core/Actor.cpp"
    replace_once(
        acpp,
        "\tvoid Actor::Initialize()\n",
        "\tconst std::vector<ActorComponent*>& Actor::GetUpdateOrderedComponents()\n\t{\n\t\tif (!updateOrderCacheDirty_) return updateOrderedComponents_;\n\t\tupdateOrderedComponents_.clear();\n\t\tupdateOrderedComponents_.reserve(components_.size());\n\t\tfor (auto& component : components_) if (component) updateOrderedComponents_.push_back(component.get());\n\t\tstd::stable_sort(updateOrderedComponents_.begin(), updateOrderedComponents_.end(),\n\t\t\t[](const ActorComponent* lhs, const ActorComponent* rhs) { return lhs->GetUpdateOrder() < rhs->GetUpdateOrder(); });\n\t\tupdateOrderCacheDirty_ = false;\n\t\treturn updateOrderedComponents_;\n\t}\n\n\tconst std::vector<ActorComponent*>& Actor::GetDrawOrderedComponents()\n\t{\n\t\tif (!drawOrderCacheDirty_) return drawOrderedComponents_;\n\t\tdrawOrderedComponents_.clear();\n\t\tdrawOrderedComponents_.reserve(components_.size());\n\t\tfor (auto& component : components_) if (component) drawOrderedComponents_.push_back(component.get());\n\t\tstd::stable_sort(drawOrderedComponents_.begin(), drawOrderedComponents_.end(),\n\t\t\t[](const ActorComponent* lhs, const ActorComponent* rhs) { return lhs->GetDrawOrder() < rhs->GetDrawOrder(); });\n\t\tdrawOrderCacheDirty_ = false;\n\t\treturn drawOrderedComponents_;\n\t}\n\n\tvoid Actor::Initialize()\n")

    text = read(acpp)
    def swap(start: str, end: str, body: str) -> None:
        nonlocal text
        s = text.find(start)
        e = text.find(end, s + len(start))
        if s < 0 or e < 0:
            raise RuntimeError(f"function range not found: {start} -> {end}")
        text = text[:s] + body + "\n\n" + text[e:]

    swap(
        "\tvoid Actor::Update(float deltaTime)",
        "\tvoid Actor::UpdateEditor(float deltaTime)",
        '''\tvoid Actor::Update(float deltaTime)\n\t{\n\t\tif (!isActive_) return;\n\t\tfor (ActorComponent* component : GetUpdateOrderedComponents())\n\t\t{\n\t\t\tif (component && component->IsActiveInHierarchy()) component->Update(deltaTime);\n\t\t}\n\t}''')
    swap(
        "\tvoid Actor::UpdateEditor(float deltaTime)",
        "\tvoid Actor::PostPhysicsUpdate(float deltaTime)",
        '''\tvoid Actor::UpdateEditor(float deltaTime)\n\t{\n\t\tif (!isActive_) return;\n\t\tfor (ActorComponent* component : GetUpdateOrderedComponents())\n\t\t{\n\t\t\tif (component && component->IsActiveInHierarchy()) component->UpdateEditor(deltaTime);\n\t\t}\n\t}''')
    swap(
        "\tvoid Actor::PostPhysicsUpdate(float deltaTime)",
        "\tvoid Actor::Draw()",
        '''\tvoid Actor::PostPhysicsUpdate(float deltaTime)\n\t{\n\t\tif (!isActive_) return;\n\t\tfor (ActorComponent* component : GetUpdateOrderedComponents())\n\t\t{\n\t\t\tif (component && component->IsActiveInHierarchy()) component->PostPhysicsUpdate(deltaTime);\n\t\t}\n\t}''')
    swap(
        "\tvoid Actor::Draw()",
        "\tvoid Actor::DrawShadow()",
        '''\tvoid Actor::Draw()\n\t{\n\t\tif (!isActive_) return;\n\t\tfor (ActorComponent* component : GetDrawOrderedComponents())\n\t\t{\n\t\t\tif (component && component->IsActiveInHierarchy()) component->Draw();\n\t\t}\n\t}''')
    swap(
        "\tvoid Actor::DrawShadow()",
        "\tvoid Actor::DrawImGui()",
        '''\tvoid Actor::DrawShadow()\n\t{\n\t\tif (!isActive_) return;\n\t\tfor (ActorComponent* component : GetDrawOrderedComponents())\n\t\t{\n\t\t\tif (component && component->IsActiveInHierarchy()) component->DrawShadow();\n\t\t}\n\t}''')
    write(acpp, text)
    replace_once(
        acpp,
        "\t\tcomponents_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する\n\t\tMarkComponentOrderDirty(true, true);\n",
        "\t\tcomponents_.clear(); // ActorがComponentの寿命を管理するため、ここで破棄する\n\t\tupdateOrderedComponents_.clear();\n\t\tdrawOrderedComponents_.clear();\n\t\tMarkComponentOrderDirty(true, true);\n")


def apply_event_driven_physics() -> None:
    awcpp = "Project/Engine/Scene/Actor/Core/ActorWorld.cpp"
    text = read(awcpp)
    old = '''\t\t\tif (!actor->IsActive())\n\t\t\t{\n\t\t\t\tUnregisterPhysicsComponents(*actor); // 無効なActorは物理登録から外す\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tactor->InitializeComponents(); // 実行中に追加されたComponentをフレーム境界で初期化する。\n\t\t\tRegisterPhysicsComponents(*actor); // ComponentのActive変更を含めてPhysics登録を同期する。\n\n\t\t\t// ActorWorldは更新順だけ管理し、処理内容はActor/Component側に任せる\n\t\t\tactor->Update(deltaTime);\n\t\t\tactor->InitializeComponents(); // Update中に追加されたComponentもPhysics登録前に初期化する。\n\t\t\tif (actor->IsPendingDestroy() || !actor->IsActive()) UnregisterPhysicsComponents(*actor);\n\t\t\telse RegisterPhysicsComponents(*actor); // Update内のActive変更を直後のPhysics Stepへ反映する。\n'''
    new = '''\t\t\tif (!actor->IsActive()) continue;\n\t\t\tactor->InitializeComponents(); // 初期化完了通知でPhysics登録をイベント同期する。\n\n\t\t\t// 毎フレームのPhysics再走査は行わず、Component追加・削除・Active変更イベントで同期する。\n\t\t\tactor->Update(deltaTime);\n\t\t\tactor->InitializeComponents(); // Update中に追加されたComponentも同フレームのPhysics Step前に初期化する。\n'''
    if old not in text:
        raise RuntimeError("ActorWorld::Update physics sync block not found")
    text = text.replace(old, new, 1)
    old2 = '''\t\t\tif (!actor->IsActive())\n\t\t\t{\n\t\t\t\tUnregisterPhysicsComponents(*actor);\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tactor->InitializeComponents();\n\t\t\tRegisterPhysicsComponents(*actor); // Edit中のActive変更も物理デバッグ登録へ反映する。\n\t\t\tactor->UpdateEditor(deltaTime);\n'''
    new2 = '''\t\t\tif (!actor->IsActive()) continue;\n\t\t\tactor->InitializeComponents();\n\t\t\tactor->UpdateEditor(deltaTime);\n'''
    if old2 not in text:
        raise RuntimeError("ActorWorld::UpdateEditor physics sync block not found")
    write(awcpp, text.replace(old2, new2, 1))

    phy = "Project/Engine/Scene/Actor/Core/ActorWorld_Pysics.cpp"
    replace_once(
        phy,
        "\t\t// 一度登録済みの静的World Actorは、417個規模のCollider登録確認を毎フレーム繰り返さない。\n\t\tif (actor.IsPhysicsRegistered() && actor.GetLayer() == \"WorldStatic\")\n\t\t{\n\t\t\treturn;\n\t\t}\n\n",
        "\t\t// Phase 2以降は毎フレーム呼ばれないため、状態変更イベント時は必ず最新構成を同期する。\n")


def apply_editor_separation() -> None:
    # 旧ActorWorld直結ImGui/Prefab/Component編集は既存ActorWorldEditorBridgeへ一本化する。
    for rel in [
        "Project/Engine/Scene/Actor/Core/ActorWorld_ImGui.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_Prefab.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_ComponentEdit.cpp",
    ]:
        p = path(rel)
        if p.exists():
            p.unlink()

    awh = "Project/Engine/Scene/Actor/Core/ActorWorld.h"
    text = read(awh)
    text = re.sub(
        r'\n\t\t/// <summary>\n\t\t/// ActorWorldが所有する全ActorのEditor表示を行う。.*?\n\t\tvoid DrawSelectedInspectorContent\(\);\n',
        '\n', text, count=1, flags=re.S)
    text = re.sub(
        r'\n\t\t/// <summary>\n\t\t/// 旧Actor World / Actor Detailsウィンドウを互換表示するか設定する。.*?\n\t\tbool IsLegacyEditorWindowsEnabled\(\) const \{ return legacyEditorWindowsEnabled_; \}\n',
        '\n', text, count=1, flags=re.S)
    # private editor helper declarations
    for name in [
        "DrawActorPrefabSpawnImGui", "RefreshActorPrefabFileList", "DrawActorPrefabBrowserImGui",
        "DrawActorPrefabSaveImGui", "DrawAddComponentImGui", "AddComponentToSelectedActor",
        "MakeUniqueComponentName", "DeleteSelectedActorPrefabFile", "IsValidActorPrefabJsonPath",
        "DeleteSelectedComponent", "DuplicateSelectedComponent",
    ]:
        text = re.sub(r'\n\t\t/// <summary>.*?\n\t\t(?:std::string|void|bool) ' + re.escape(name) + r'\([^;]*\);\n', '\n', text, count=1, flags=re.S)
    # editor-only state
    for pattern in [
        r'\n\t\t// Actor Detailsウィンドウへフォーカスを移す要求\n\t\tbool requestFocusActorDetails_ = false;\n',
        r'\n\t\t// 旧Actor World / Actor Detailsウィンドウを表示する互換フラグ\n\t\tbool legacyEditorWindowsEnabled_ = false;\n',
        r'\n\t\tVector3 actorPrefabSpawnOffset_ = \{ 3\.0f, 0\.0f, 0\.0f \}; // Actor Prefab Spawn時の位置オフセット\n',
        r'\n\t\t// Editorから追加するComponentの種類選択用\n\t\tint selectedAddComponentTypeIndex_ = 0;\n',
        r'\n\t\t// Actor Prefabフォルダ内で見つかったJSONファイル一覧\n\t\tstd::vector<std::string> actorPrefabFiles_;\n',
        r'\n\t\t// Actor Prefabフォルダのパス\n\t\tstd::string actorPrefabDirectory_ = "Resources/ActorPrefabs";\n',
        r'\n\t\t// Actor PrefabのJSONパス入力用バッファ\n\t\tstd::string actorPrefabPath_ = "Resources/ActorPrefabs/TestActor.json";\n',
        r'\n\t\t// 選択中ActorをPrefabとして保存する際のデフォルトパス\n\t\tstd::string actorPrefabSavePath_ = "Resources/ActorPrefabs/NewActorPrefab.json";\n',
    ]:
        text = re.sub(pattern, '\n', text, count=1)
    write(awh, text)

    awcpp = "Project/Engine/Scene/Actor/Core/ActorWorld.cpp"
    text = read(awcpp)
    text = text.replace("\n\t\tRefreshActorPrefabFileList(); // Actor PrefabsのJSONファイル一覧を更新する\n", "\n")
    write(awcpp, text)

    debug = "Project/ApplicationLayer/Scene/DebugScene/DebugScene.cpp"
    replace_once(debug, "\tactorWorld_.DrawImGui();\n", "\t// ActorWorldのEditor UIはActorWorldEditorBridgeへ分離済み。\n")

    bridge = "Project/Engine/Editor/ActorWorldEditorBridge.cpp"
    text = read(bridge)
    text = text.replace(
        "static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor))",
        "static_cast<unsigned long long>(actor ? actor->GetId().value : 0)")
    text = text.replace(
        "std::to_string(reinterpret_cast<uintptr_t>(actor))",
        "std::to_string(actor ? actor->GetId().value : 0)")
    text = text.replace(
        "static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(component))",
        "static_cast<unsigned long long>(component ? component->GetId().value : 0)")
    text = text.replace(
        "std::to_string(reinterpret_cast<uintptr_t>(component))",
        "std::to_string(component ? component->GetId().value : 0)")
    write(bridge, text)

    needles = [
        "ActorWorld_ComponentEdit.cpp", "ActorWorld_ImGui.cpp", "ActorWorld_Prefab.cpp"
    ]
    remove_lines_containing("Project/Ken4lowEngine.vcxproj", needles)
    remove_lines_containing("Project/Ken4lowEngine.vcxproj.filters", needles)

    # 新しいRuntime IDヘッダーをVisual Studio上でも追えるようActor.hの近くへ登録する。
    proj = "Project/Ken4lowEngine.vcxproj"
    ptext = read(proj)
    if "Engine\\Scene\\Actor\\Core\\WorldObjectId.h" not in ptext:
        anchor = '    <ClInclude Include="Engine\\Scene\\Actor\\Core\\Actor.h" />\n'
        if anchor not in ptext:
            raise RuntimeError("Actor.h ClInclude anchor not found")
        ptext = ptext.replace(anchor, anchor + '    <ClInclude Include="Engine\\Scene\\Actor\\Core\\ActorHandle.h" />\n    <ClInclude Include="Engine\\Scene\\Actor\\Core\\WorldObjectId.h" />\n', 1)
        write(proj, ptext)


def apply_docs() -> None:
    write(
        "Project/Docs/Phase2WorldFoundation.md",
        r'''# Phase 2 — World基盤

## 完了項目

- [x] ActorId / ComponentId
- [x] Component ordered-cache
- [x] Physics登録をevent-driven化
- [x] ActorWorldからEditor UI / Prefab / Component編集コードを分離
- [x] Actor参照Handle

## Runtime ID

`ActorId` / `ComponentId` は `ActorWorld` が単調増加で発行する実行時IDです。`0` は無効値です。
IDはJSONへ永続化しません。Duplicate / JSON Spawn / Component再生成では新しいRuntime IDを割り当てます。
ActorWorldから外れた時点でlookup tableから削除するため、古いHandleは解決できません。

## ActorHandle

`ActorHandle` はActorの生ポインタやActorWorld自体を所有せず、ActorIdだけを保持します。
`ActorWorld::ResolveActor()` で必要な瞬間に解決し、Destroy済み / World外のActorは無効になります。

## Component ordered-cache

Update / PostPhysics / EditorUpdateは共通のUpdate順キャッシュ、Draw / DrawShadowはDraw順キャッシュを利用します。
Component追加・削除・UpdateOrder / DrawOrder変更時だけcacheをdirtyにして再構築します。
毎フレームの一時vector生成とstable_sortは行いません。

## Physics event-driven

毎フレームActor全体のCollider / Rigidbodyを再走査する方式を廃止しました。
以下のイベントでPhysicsWorldとの登録状態を同期します。

- Component追加
- Component初期化完了
- Component削除
- Component Active変更
- Actor Active変更
- Actor Spawn / Destroy / JSON Reload
- PhysicsWorld差し替え

状態変更イベントではActor単位で最新のPhysics構成を再同期し、通常フレームでは登録走査を行いません。

## Editor分離

旧 `ActorWorld_ImGui.cpp` / `ActorWorld_Prefab.cpp` / `ActorWorld_ComponentEdit.cpp` を削除し、通常Editor導線は既存 `ActorWorldEditorBridge` に統一しました。
Editorのobject ID生成もポインタ値ではなくActorId / ComponentIdを優先します。
ActorWorldにはWorld Outlinerとの選択同期など最小限の橋渡しだけ残します。

## Phase 2完了条件

- Runtime ID lookupがSpawn / Destroy / Component追加削除に追従する
- Destroy後のActorHandleが解決されない
- Actor Update / Drawで毎フレームsortしない
- Physics登録走査が毎フレーム実行されない
- 旧ActorWorld直結Editorウィンドウ実装がRuntime Coreから除去されている
- Debug / ReleaseでC++コンパイルできる
''')


def validate_sources() -> None:
    actor_cpp = read("Project/Engine/Scene/Actor/Core/Actor.cpp")
    world_cpp = read("Project/Engine/Scene/Actor/Core/ActorWorld.cpp")
    world_h = read("Project/Engine/Scene/Actor/Core/ActorWorld.h")
    physics_cpp = read("Project/Engine/Scene/Actor/Core/ActorWorld_Pysics.cpp")

    required = [
        "ActorWorld::FindActorById", "ActorWorld::ResolveActor", "ActorWorld::OnComponentAdded",
        "Actor::GetUpdateOrderedComponents", "Actor::GetDrawOrderedComponents",
        "NotifyOwnerRuntimeStateChanged", "ActorHandle",
    ]
    joined = actor_cpp + world_cpp + world_h + read("Project/Engine/Scene/Actor/Core/ActorComponent.h")
    missing = [item for item in required if item not in joined]
    if missing:
        raise RuntimeError(f"missing Phase2 markers: {missing}")

    update_start = world_cpp.find("void ActorWorld::Update(float deltaTime)")
    update_end = world_cpp.find("void ActorWorld::UpdateEditor", update_start)
    update_body = world_cpp[update_start:update_end]
    if "RegisterPhysicsComponents(*actor)" in update_body or "UnregisterPhysicsComponents(*actor)" in update_body:
        raise RuntimeError("per-frame physics registration remains in ActorWorld::Update")

    if "actor.IsPhysicsRegistered() && actor.GetLayer() == \"WorldStatic\"" in physics_cpp:
        raise RuntimeError("old static physics early-out remains")

    for rel in [
        "Project/Engine/Scene/Actor/Core/ActorWorld_ImGui.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_Prefab.cpp",
        "Project/Engine/Scene/Actor/Core/ActorWorld_ComponentEdit.cpp",
    ]:
        if path(rel).exists():
            raise RuntimeError(f"legacy editor file still exists: {rel}")

    if "DrawActorPrefabSpawnImGui" in world_h or "legacyEditorWindowsEnabled_" in world_h:
        raise RuntimeError("ActorWorld editor implementation declarations remain")

    print("Phase 2 source audit: OK")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: ApplyPhase2WorldFoundation.py <ids|cache|physics|editor|docs|validate>")
    stage = sys.argv[1]
    if stage == "ids":
        apply_ids_and_handles()
    elif stage == "cache":
        apply_order_cache()
    elif stage == "physics":
        apply_event_driven_physics()
    elif stage == "editor":
        apply_editor_separation()
    elif stage == "docs":
        apply_docs()
    elif stage == "validate":
        validate_sources()
    else:
        raise SystemExit(f"unknown stage: {stage}")


if __name__ == "__main__":
    main()
