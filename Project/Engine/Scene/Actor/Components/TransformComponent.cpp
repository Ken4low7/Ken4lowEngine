#include "TransformComponent.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

void Ken4lowEngine::TransformComponent::DrawImGui()
{
#ifdef USE_IMGUI
	// ActorのTransform情報をDebugScene上で確認・編集できるようにする
	ImGui::DragFloat3("Position", &position_.x, 0.1f);
	ImGui::DragFloat3("Rotation", &rotation_.x, 0.1f);
	ImGui::DragFloat3("Scale", &scale_.x, 0.1f);
#endif // USE_IMGUI
}
