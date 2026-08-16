#include "ActorComponent.h"

#include "Actor.h"
#include "ActorHandle.h"
#include "ActorWorld.h"

namespace Ken4lowEngine
{

ActorHandle ActorComponent::GetOwnerHandle() const
{
	if (!owner_ || !owner_->world_) return {};
	return owner_->world_->MakeActorHandle(owner_);
}

} // namespace Ken4lowEngine
