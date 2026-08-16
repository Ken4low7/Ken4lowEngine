#include "VfxGraphEditor.h"

#include "Engine/Vfx/Graph/Asset/VfxGraphSerializer.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphCompiler.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <unordered_map>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
namespace
{
	constexpr std::array<VfxGraphNodeType, 25> kNodeTypes = {
		VfxGraphNodeType::SpawnRate,
		VfxGraphNodeType::Burst,
		VfxGraphNodeType::SpawnPoint,
		VfxGraphNodeType::SpawnSphere,
		VfxGraphNodeType::SpawnBox,
		VfxGraphNodeType::Lifetime,
		VfxGraphNodeType::InitialVelocity,
		VfxGraphNodeType::InitialColor,
		VfxGraphNodeType::InitialSize,
		VfxGraphNodeType::Gravity,
		VfxGraphNodeType::Drag,
		VfxGraphNodeType::InitialRotation,
		VfxGraphNodeType::RotationRate,
		VfxGraphNodeType::SizeOverLife,
		VfxGraphNodeType::ColorOverLife,
		VfxGraphNodeType::Collision,
		VfxGraphNodeType::DeathEvent,
		VfxGraphNodeType::SubEmitter,
		VfxGraphNodeType::SpriteRenderer,
		VfxGraphNodeType::RibbonRenderer,
		VfxGraphNodeType::TrailRenderer,
		VfxGraphNodeType::MeshRenderer,
		VfxGraphNodeType::FluidOutput,
		VfxGraphNodeType::LightOutput,
		VfxGraphNodeType::PostEffectOutput,
	};

	VfxGraphNodePayload MakeDefaultPayload(VfxGraphNodeType type)
	{
		switch (type)
		{
		case VfxGraphNodeType::SpawnRate: return VfxGraphSpawnRateNode{};
		case VfxGraphNodeType::Burst: return VfxGraphBurstNode{};
		case VfxGraphNodeType::SpawnPoint: return VfxGraphSpawnPointNode{};
		case VfxGraphNodeType::SpawnSphere: return VfxGraphSpawnSphereNode{};
		case VfxGraphNodeType::SpawnBox: return VfxGraphSpawnBoxNode{};
		case VfxGraphNodeType::Lifetime: return VfxGraphLifetimeNode{};
		case VfxGraphNodeType::InitialVelocity: return VfxGraphInitialVelocityNode{};
		case VfxGraphNodeType::InitialColor: return VfxGraphInitialColorNode{};
		case VfxGraphNodeType::InitialSize: return VfxGraphInitialSizeNode{};
		case VfxGraphNodeType::Gravity: return VfxGraphGravityNode{};
		case VfxGraphNodeType::Drag: return VfxGraphDragNode{};
		case VfxGraphNodeType::InitialRotation: return VfxGraphInitialRotationNode{};
		case VfxGraphNodeType::RotationRate: return VfxGraphRotationRateNode{};
		case VfxGraphNodeType::SizeOverLife: return VfxGraphSizeOverLifeNode{};
		case VfxGraphNodeType::ColorOverLife: return VfxGraphColorOverLifeNode{};
		case VfxGraphNodeType::Collision: return VfxGraphCollisionNode{};
		case VfxGraphNodeType::DeathEvent: return VfxGraphDeathEventNode{};
		case VfxGraphNodeType::SubEmitter: return VfxGraphSubEmitterNode{};
		case VfxGraphNodeType::SpriteRenderer: return VfxGraphSpriteRendererNode{};
		case VfxGraphNodeType::RibbonRenderer: return VfxGraphRibbonRendererNode{};
		case VfxGraphNodeType::TrailRenderer: return VfxGraphTrailRendererNode{};
		case VfxGraphNodeType::MeshRenderer: return VfxGraphMeshRendererNode{};
		case VfxGraphNodeType::FluidOutput: return VfxGraphFluidOutputNode{};
		case VfxGraphNodeType::LightOutput: return VfxGraphLightOutputNode{};
		case VfxGraphNodeType::PostEffectOutput: return VfxGraphPostEffectOutputNode{};
		default: return VfxGraphSpawnRateNode{};
		}
	}

#ifdef USE_IMGUI
	bool DrawBlendMode(const char* label, GpuParticleBlendMode& mode)
	{
		static const char* kNames[] = { "Alpha", "Additive", "Multiply" };
		int value = static_cast<int>(mode);
		if (!ImGui::Combo(label, &value, kNames, IM_ARRAYSIZE(kNames)))
		{
			return false;
		}
		mode = static_cast<GpuParticleBlendMode>(value);
		return true;
	}
#endif // USE_IMGUI
}

VfxGraphEditor* VfxGraphEditor::GetInstance()
{
	static VfxGraphEditor instance;
	return &instance;
}

void VfxGraphEditor::Initialize()
{
	if (initialized_)
	{
		return;
	}

	initialized_ = true;
	if (!LoadFromDisk())
	{
		editableGraph_ = {};
		editableGraph_.graphName = "Phase25EditorGraph";
		editableGraph_.emitters.push_back(VfxGraphEmitterDesc{});
		lastMessage_ = "Created an empty graph because the default Phase25 asset could not be loaded.";
	}
	CompileEditableGraph(false);
}

void VfxGraphEditor::Finalize()
{
	StopPreview();
	editableGraph_ = {};
	compileResult_ = {};
	initialized_ = false;
	dirty_ = false;
}

bool VfxGraphEditor::LoadFromDisk()
{
	VfxGraphDesc loaded{};
	if (!VfxGraphSerializer::Load(loaded, filePath_))
	{
		lastMessage_ = "Load failed: " + filePath_;
		return false;
	}

	StopPreview();
	editableGraph_ = std::move(loaded);
	selectedEmitterIndex_ = 0u;
	selectedNodeId_ = 0u;
	pendingEdgeFromNodeId_ = 0u;
	for (auto& emitter : editableGraph_.emitters)
	{
		EnsureEditorLayout(emitter);
	}
	CompileEditableGraph(false);
	dirty_ = false;
	lastMessage_ = "Loaded: " + filePath_;
	return true;
}

bool VfxGraphEditor::SaveToDisk()
{
	if (!VfxGraphSerializer::Save(editableGraph_, filePath_))
	{
		lastMessage_ = "Save failed: " + filePath_;
		return false;
	}
	dirty_ = false;
	lastMessage_ = "Saved: " + filePath_;
	return true;
}

bool VfxGraphEditor::CompileEditableGraph(bool restartPreview)
{
	compileResult_ = VfxGraphCompiler::Compile(editableGraph_);
	if (!compileResult_.success)
	{
		lastMessage_ = "Compile failed with " + std::to_string(compileResult_.errors.size()) + " error(s).";
		return false;
	}

	lastMessage_ = "Compile succeeded.";
	if (restartPreview && previewHandle_.IsValid())
	{
		if (!VfxGraphRuntime::GetInstance()->RegisterGraph(editableGraph_))
		{
			lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
			return false;
		}

		VfxGraphRuntime::GetInstance()->StopLoop(previewHandle_);
		previewHandle_ = VfxGraphRuntime::GetInstance()->PlayLoop(editableGraph_.graphName, previewPosition_);
		if (!previewHandle_.IsValid())
		{
			lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
			return false;
		}
		lastMessage_ = "Live preview recompiled.";
	}
	return true;
}

bool VfxGraphEditor::StartPreview()
{
	if (!CompileEditableGraph(false))
	{
		return false;
	}
	if (!VfxGraphRuntime::GetInstance()->RegisterGraph(editableGraph_))
	{
		lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
		return false;
	}

	StopPreview();
	previewHandle_ = VfxGraphRuntime::GetInstance()->PlayLoop(editableGraph_.graphName, previewPosition_);
	if (!previewHandle_.IsValid())
	{
		lastMessage_ = VfxGraphRuntime::GetInstance()->GetLastStatus();
		return false;
	}
	lastMessage_ = "Preview started.";
	return true;
}

void VfxGraphEditor::StopPreview()
{
	if (previewHandle_.IsValid())
	{
		VfxGraphRuntime::GetInstance()->StopLoop(previewHandle_);
		previewHandle_ = {};
	}
}

void VfxGraphEditor::MarkGraphDirty()
{
	// Authoring changes are compiled by the existing Phase20 compiler instead of creating an editor-only execution path.
	dirty_ = true;
}

VfxGraphEmitterDesc* VfxGraphEditor::GetSelectedEmitter()
{
	if (selectedEmitterIndex_ >= editableGraph_.emitters.size())
	{
		return nullptr;
	}
	return &editableGraph_.emitters[selectedEmitterIndex_];
}

VfxGraphNodeDesc* VfxGraphEditor::GetSelectedNode()
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || selectedNodeId_ == 0u)
	{
		return nullptr;
	}
	for (auto& node : emitter->nodes)
	{
		if (node.id == selectedNodeId_)
		{
			return &node;
		}
	}
	return nullptr;
}

uint32_t VfxGraphEditor::AllocateNodeId(const VfxGraphEmitterDesc& emitter) const
{
	uint32_t result = 1u;
	for (const auto& node : emitter.nodes)
	{
		result = (std::max)(result, node.id + 1u);
	}
	return result;
}

void VfxGraphEditor::AddNode(VfxGraphNodeType type)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || emitter->nodes.size() >= VfxGraphDesc::kMaxNodesPerEmitter)
	{
		return;
	}

	VfxGraphNodeDesc node{};
	node.id = AllocateNodeId(*emitter);
	node.type = type;
	node.stage = GetExpectedVfxGraphNodeStage(type);
	node.name = ToString(type);
	node.payload = MakeDefaultPayload(type);
	node.editorPosition = { static_cast<float>(static_cast<uint32_t>(node.stage)) * 250.0f, 80.0f };
	emitter->nodes.push_back(std::move(node));
	selectedNodeId_ = emitter->nodes.back().id;
	MarkGraphDirty();
}

void VfxGraphEditor::RemoveSelectedNode()
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || selectedNodeId_ == 0u)
	{
		return;
	}

	const uint32_t removedId = selectedNodeId_;
	emitter->nodes.erase(
		std::remove_if(emitter->nodes.begin(), emitter->nodes.end(), [removedId](const VfxGraphNodeDesc& node)
			{
				return node.id == removedId;
			}),
		emitter->nodes.end());
	emitter->edges.erase(
		std::remove_if(emitter->edges.begin(), emitter->edges.end(), [removedId](const VfxGraphEdgeDesc& edge)
			{
				return edge.fromNodeId == removedId || edge.toNodeId == removedId;
			}),
		emitter->edges.end());
	selectedNodeId_ = 0u;
	if (pendingEdgeFromNodeId_ == removedId)
	{
		pendingEdgeFromNodeId_ = 0u;
	}
	MarkGraphDirty();
}

void VfxGraphEditor::AddEdge(uint32_t fromNodeId, uint32_t toNodeId)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter || fromNodeId == 0u || toNodeId == 0u || fromNodeId == toNodeId || emitter->edges.size() >= VfxGraphDesc::kMaxEdgesPerEmitter)
	{
		return;
	}
	const auto duplicate = std::find_if(emitter->edges.begin(), emitter->edges.end(), [fromNodeId, toNodeId](const VfxGraphEdgeDesc& edge)
		{
			return edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId;
		});
	if (duplicate != emitter->edges.end())
	{
		return;
	}
	emitter->edges.push_back({ fromNodeId, toNodeId });
	MarkGraphDirty();
}

void VfxGraphEditor::RemoveEdge(uint32_t fromNodeId, uint32_t toNodeId)
{
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		return;
	}
	const auto oldSize = emitter->edges.size();
	emitter->edges.erase(
		std::remove_if(emitter->edges.begin(), emitter->edges.end(), [fromNodeId, toNodeId](const VfxGraphEdgeDesc& edge)
			{
				return edge.fromNodeId == fromNodeId && edge.toNodeId == toNodeId;
			}),
		emitter->edges.end());
	if (oldSize != emitter->edges.size())
	{
		MarkGraphDirty();
	}
}

void VfxGraphEditor::EnsureEditorLayout(VfxGraphEmitterDesc& emitter)
{
	std::array<uint32_t, 4> stageRows{};
	for (auto& node : emitter.nodes)
	{
		const uint32_t stageIndex = static_cast<uint32_t>(node.stage);
		if (node.editorPosition.x == 0.0f && node.editorPosition.y == 0.0f)
		{
			node.editorPosition.x = static_cast<float>(stageIndex) * 250.0f;
			node.editorPosition.y = static_cast<float>(stageRows[stageIndex]) * 92.0f;
		}
		++stageRows[stageIndex];
	}
}

void VfxGraphEditor::Draw(bool* open)
{
#ifdef USE_IMGUI
	if (!initialized_)
	{
		Initialize();
	}
	if (!ImGui::Begin("VFX Graph Editor", open, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	DrawToolbar();
	DrawGraphHeader();

	if (ImGui::BeginTable("##VfxGraphEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Emitters", ImGuiTableColumnFlags_WidthFixed, 190.0f);
		ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 340.0f);
		ImGui::TableNextColumn();
		DrawEmitterList();
		ImGui::TableNextColumn();
		DrawGraphCanvas();
		ImGui::TableNextColumn();
		DrawNodeInspector();
		ImGui::EndTable();
	}

	DrawPreviewPanel();
	DrawCompileDiagnostics();
	ImGui::TextDisabled("%s", lastMessage_.c_str());
	ImGui::End();

	if (dirty_ && liveCompile_)
	{
		dirty_ = false;
		CompileEditableGraph(livePreview_ && previewHandle_.IsValid());
	}
#else
	(void)open;
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawToolbar()
{
#ifdef USE_IMGUI
	char pathBuffer[512]{};
	std::snprintf(pathBuffer, sizeof(pathBuffer), "%s", filePath_.c_str());
	ImGui::SetNextItemWidth(420.0f);
	if (ImGui::InputText("Asset", pathBuffer, sizeof(pathBuffer)))
	{
		filePath_ = pathBuffer;
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		LoadFromDisk();
	}
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		SaveToDisk();
	}
	ImGui::SameLine();
	if (ImGui::Button("Compile"))
	{
		dirty_ = false;
		CompileEditableGraph(livePreview_ && previewHandle_.IsValid());
	}
	ImGui::SameLine();
	ImGui::Checkbox("Live Compile", &liveCompile_);
	ImGui::SameLine();
	ImGui::Checkbox("Live Preview", &livePreview_);
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawGraphHeader()
{
#ifdef USE_IMGUI
	char graphName[160]{};
	std::snprintf(graphName, sizeof(graphName), "%s", editableGraph_.graphName.c_str());
	if (ImGui::InputText("Graph Name", graphName, sizeof(graphName)))
	{
		editableGraph_.graphName = graphName;
		MarkGraphDirty();
	}
	ImGui::SameLine();
	ImGui::Text("Emitters: %d", static_cast<int>(editableGraph_.emitters.size()));
	if (ImGui::TreeNode("Phase27 Scalability"))
	{
		auto& scalability = editableGraph_.scalability;
		const char* boundsModes[] = { "Automatic", "FixedSphere" };
		int boundsMode = static_cast<int>(scalability.boundsMode);
		if (ImGui::Combo("Bounds Mode", &boundsMode, boundsModes, IM_ARRAYSIZE(boundsModes))) { scalability.boundsMode = static_cast<VfxGraphBoundsMode>(boundsMode); MarkGraphDirty(); }
		if (scalability.boundsMode == VfxGraphBoundsMode::FixedSphere)
		{
			if (ImGui::DragFloat3("Bounds Center", &scalability.fixedBoundsCenter.x, 0.05f)) MarkGraphDirty();
			if (ImGui::DragFloat("Bounds Radius", &scalability.fixedBoundsRadius, 0.05f, 0.1f, 10000.0f)) MarkGraphDirty();
		}
		if (ImGui::Checkbox("Frustum Culling", &scalability.frustumCulling)) MarkGraphDirty();
		if (ImGui::DragFloat("Max Draw Distance", &scalability.maxDrawDistance, 1.0f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::DragFloat("LOD Near", &scalability.lodNearDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::DragFloat("LOD Far", &scalability.lodFarDistance, 0.5f, 0.0f, 100000.0f)) MarkGraphDirty();
		if (ImGui::SliderFloat("LOD Mid Scale", &scalability.lodMidScale, 0.05f, 1.0f)) MarkGraphDirty();
		if (ImGui::SliderFloat("LOD Far Scale", &scalability.lodFarScale, 0.01f, 1.0f)) MarkGraphDirty();
		int budgetCost = static_cast<int>(scalability.budgetCost);
		if (ImGui::DragInt("Budget Cost", &budgetCost, 1.0f, 1, 64)) { scalability.budgetCost = static_cast<uint32_t>((std::max)(budgetCost, 1)); MarkGraphDirty(); }
		ImGui::TreePop();
	}
	ImGui::Separator();
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawEmitterList()
{
#ifdef USE_IMGUI
	ImGui::TextUnformatted("Emitters");
	for (uint32_t i = 0; i < editableGraph_.emitters.size(); ++i)
	{
		const bool selected = i == selectedEmitterIndex_;
		if (ImGui::Selectable(editableGraph_.emitters[i].name.c_str(), selected))
		{
			selectedEmitterIndex_ = i;
			selectedNodeId_ = 0u;
			pendingEdgeFromNodeId_ = 0u;
		}
	}
	if (ImGui::Button("+ Emitter") && editableGraph_.emitters.size() < VfxGraphDesc::kMaxEmitters)
	{
		VfxGraphEmitterDesc emitter{};
		emitter.name = "Emitter" + std::to_string(editableGraph_.emitters.size() + 1u);
		editableGraph_.emitters.push_back(std::move(emitter));
		selectedEmitterIndex_ = static_cast<uint32_t>(editableGraph_.emitters.size() - 1u);
		selectedNodeId_ = 0u;
		MarkGraphDirty();
	}
	ImGui::SameLine();
	if (ImGui::Button("- Emitter") && editableGraph_.emitters.size() > 1u && selectedEmitterIndex_ < editableGraph_.emitters.size())
	{
		editableGraph_.emitters.erase(editableGraph_.emitters.begin() + selectedEmitterIndex_);
		selectedEmitterIndex_ = std::min<uint32_t>(selectedEmitterIndex_, static_cast<uint32_t>(editableGraph_.emitters.size() - 1u));
		selectedNodeId_ = 0u;
		MarkGraphDirty();
	}

	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		return;
	}
	ImGui::Separator();
	char nameBuffer[128]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", emitter->name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		emitter->name = nameBuffer;
		MarkGraphDirty();
	}
	int maxParticles = static_cast<int>(emitter->maxParticles);
	if (ImGui::DragInt("Max", &maxParticles, 16.0f, 1, 131072))
	{
		emitter->maxParticles = static_cast<uint32_t>((std::max)(maxParticles, 1));
		MarkGraphDirty();
	}
	if (ImGui::Checkbox("Loop", &emitter->loop))
	{
		MarkGraphDirty();
	}
	if (ImGui::DragFloat("Duration", &emitter->duration, 0.05f, 0.01f, 60.0f))
	{
		MarkGraphDirty();
	}

	ImGui::SeparatorText("Add Node");
	const char* preview = ToString(kNodeTypes[static_cast<size_t>(std::clamp(addNodeType_, 0, static_cast<int>(kNodeTypes.size()) - 1))]);
	if (ImGui::BeginCombo("Type", preview))
	{
		for (int i = 0; i < static_cast<int>(kNodeTypes.size()); ++i)
		{
			const bool selected = addNodeType_ == i;
			if (ImGui::Selectable(ToString(kNodeTypes[static_cast<size_t>(i)]), selected))
			{
				addNodeType_ = i;
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::Button("Add Node", ImVec2(-1.0f, 0.0f)))
	{
		AddNode(kNodeTypes[static_cast<size_t>(addNodeType_)]);
	}
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawGraphCanvas()
{
#ifdef USE_IMGUI
	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (!emitter)
	{
		ImGui::TextDisabled("No emitter selected.");
		return;
	}

	ImGui::Text("Graph Canvas  |  Zoom %.0f%%  |  Edges %d", canvasZoom_ * 100.0f, static_cast<int>(emitter->edges.size()));
	ImGui::SameLine();
	if (ImGui::SmallButton("Frame All"))
	{
		canvasPan_ = { 20.0f, 50.0f };
		canvasZoom_ = 1.0f;
	}

	ImGui::BeginChild("##VfxGraphCanvas", ImVec2(0.0f, 460.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(canvasMin, canvasMax, ImGui::GetColorU32(ImGuiCol_WindowBg));

	const float gridStep = 32.0f * canvasZoom_;
	if (gridStep >= 10.0f)
	{
		for (float x = std::fmod(canvasPan_.x, gridStep); x < canvasSize.x; x += gridStep)
		{
			drawList->AddLine(ImVec2(canvasMin.x + x, canvasMin.y), ImVec2(canvasMin.x + x, canvasMax.y), ImGui::GetColorU32(ImGuiCol_Border, 0.25f));
		}
		for (float y = std::fmod(canvasPan_.y, gridStep); y < canvasSize.y; y += gridStep)
		{
			drawList->AddLine(ImVec2(canvasMin.x, canvasMin.y + y), ImVec2(canvasMax.x, canvasMin.y + y), ImGui::GetColorU32(ImGuiCol_Border, 0.25f));
		}
	}

	const bool canvasHovered = ImGui::IsWindowHovered();
	ImGuiIO& io = ImGui::GetIO();
	if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		canvasPan_.x += io.MouseDelta.x;
		canvasPan_.y += io.MouseDelta.y;
	}
	if (canvasHovered && io.KeyCtrl && io.MouseWheel != 0.0f)
	{
		canvasZoom_ = std::clamp(canvasZoom_ + io.MouseWheel * 0.1f, 0.35f, 2.0f);
	}

	constexpr float kNodeWidth = 178.0f;
	constexpr float kNodeHeight = 64.0f;
	std::unordered_map<uint32_t, ImVec2> nodeMinById;
	for (const auto& node : emitter->nodes)
	{
		nodeMinById[node.id] = ImVec2(
			canvasMin.x + canvasPan_.x + node.editorPosition.x * canvasZoom_,
			canvasMin.y + canvasPan_.y + node.editorPosition.y * canvasZoom_);
	}

	for (const auto& edge : emitter->edges)
	{
		const auto fromIt = nodeMinById.find(edge.fromNodeId);
		const auto toIt = nodeMinById.find(edge.toNodeId);
		if (fromIt == nodeMinById.end() || toIt == nodeMinById.end())
		{
			continue;
		}
		const ImVec2 from(fromIt->second.x + kNodeWidth * canvasZoom_, fromIt->second.y + kNodeHeight * 0.5f * canvasZoom_);
		const ImVec2 to(toIt->second.x, toIt->second.y + kNodeHeight * 0.5f * canvasZoom_);
		const float tangent = (std::max)(45.0f, std::abs(to.x - from.x) * 0.4f);
		drawList->AddBezierCubic(from, ImVec2(from.x + tangent, from.y), ImVec2(to.x - tangent, to.y), to, ImGui::GetColorU32(ImGuiCol_PlotLines), 2.0f);
	}

	for (auto& node : emitter->nodes)
	{
		ImGui::PushID(static_cast<int>(node.id));
		ImVec2 nodeMin = nodeMinById[node.id];
		const ImVec2 nodeSize(kNodeWidth * canvasZoom_, kNodeHeight * canvasZoom_);
		const ImVec2 nodeMax(nodeMin.x + nodeSize.x, nodeMin.y + nodeSize.y);
		ImGui::SetCursorScreenPos(nodeMin);
		ImGui::InvisibleButton("##node", nodeSize, ImGuiButtonFlags_MouseButtonLeft);
		if (ImGui::IsItemClicked())
		{
			selectedNodeId_ = node.id;
		}
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			node.editorPosition.x += io.MouseDelta.x / canvasZoom_;
			node.editorPosition.y += io.MouseDelta.y / canvasZoom_;
			MarkGraphDirty();
		}

		const bool selected = selectedNodeId_ == node.id;
		const ImU32 fill = ImGui::GetColorU32(selected ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg);
		const ImU32 border = ImGui::GetColorU32(selected ? ImGuiCol_CheckMark : ImGuiCol_Border);
		drawList->AddRectFilled(nodeMin, nodeMax, fill, 5.0f);
		drawList->AddRect(nodeMin, nodeMax, border, 5.0f, 0, selected ? 2.0f : 1.0f);
		drawList->AddCircleFilled(ImVec2(nodeMin.x, nodeMin.y + nodeSize.y * 0.5f), 5.0f, ImGui::GetColorU32(ImGuiCol_PlotLines));
		drawList->AddCircleFilled(ImVec2(nodeMax.x, nodeMin.y + nodeSize.y * 0.5f), 5.0f, ImGui::GetColorU32(ImGuiCol_PlotLines));
		drawList->AddText(ImVec2(nodeMin.x + 10.0f, nodeMin.y + 8.0f), ImGui::GetColorU32(ImGuiCol_Text), node.name.c_str());
		const std::string subtitle = std::string(ToString(node.stage)) + " / " + ToString(node.type) + (node.enabled ? "" : " [disabled]");
		drawList->AddText(ImVec2(nodeMin.x + 10.0f, nodeMin.y + 31.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), subtitle.c_str());
		ImGui::PopID();
	}

	ImGui::EndChild();
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawNodeInspector()
{
#ifdef USE_IMGUI
	VfxGraphNodeDesc* node = GetSelectedNode();
	if (!node)
	{
		ImGui::TextDisabled("Select a node to edit its module parameters.");
		return;
	}

	ImGui::SeparatorText("Node Inspector");
	char nameBuffer[160]{};
	std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", node->name.c_str());
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		node->name = nameBuffer;
		MarkGraphDirty();
	}
	ImGui::Text("ID: %u", node->id);
	ImGui::Text("Stage: %s", ToString(node->stage));
	ImGui::Text("Type: %s", ToString(node->type));
	if (ImGui::Checkbox("Enabled", &node->enabled))
	{
		MarkGraphDirty();
	}
	if (ImGui::DragFloat2("Editor Position", &node->editorPosition.x, 1.0f))
	{
		MarkGraphDirty();
	}

	if (DrawNodePayloadEditor(*node))
	{
		MarkGraphDirty();
	}

	ImGui::SeparatorText("Connections");
	if (pendingEdgeFromNodeId_ == 0u)
	{
		if (ImGui::Button("Start Connection From This Node"))
		{
			pendingEdgeFromNodeId_ = node->id;
		}
	}
	else
	{
		ImGui::Text("From node: %u", pendingEdgeFromNodeId_);
		if (pendingEdgeFromNodeId_ != node->id && ImGui::Button("Connect To This Node"))
		{
			AddEdge(pendingEdgeFromNodeId_, node->id);
			pendingEdgeFromNodeId_ = 0u;
		}
		if (ImGui::Button("Cancel Connection"))
		{
			pendingEdgeFromNodeId_ = 0u;
		}
	}

	VfxGraphEmitterDesc* emitter = GetSelectedEmitter();
	if (emitter)
	{
		int edgeIndex = 0;
		for (const auto& edge : emitter->edges)
		{
			if (edge.fromNodeId != node->id && edge.toNodeId != node->id)
			{
				continue;
			}
			ImGui::PushID(edgeIndex++);
			ImGui::Text("%u -> %u", edge.fromNodeId, edge.toNodeId);
			ImGui::SameLine();
			if (ImGui::SmallButton("x"))
			{
				const uint32_t from = edge.fromNodeId;
				const uint32_t to = edge.toNodeId;
				ImGui::PopID();
				RemoveEdge(from, to);
				break;
			}
			ImGui::PopID();
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Delete Node", ImVec2(-1.0f, 0.0f)))
	{
		RemoveSelectedNode();
	}
#endif // USE_IMGUI
}

bool VfxGraphEditor::DrawNodePayloadEditor(VfxGraphNodeDesc& node)
{
#ifdef USE_IMGUI
	bool changed = false;
	auto editString = [&changed](const char* label, std::string& value)
		{
			char buffer[256]{};
			std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
			if (ImGui::InputText(label, buffer, sizeof(buffer)))
			{
				value = buffer;
				changed = true;
			}
		};

	if (auto* spawnRate = std::get_if<VfxGraphSpawnRateNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Rate", &spawnRate->rate, 0.1f, 0.0f, 100000.0f) || changed;
	}
	else if (auto* burst = std::get_if<VfxGraphBurstNode>(&node.payload))
	{
		int value = static_cast<int>(burst->count);
		if (ImGui::DragInt("Count", &value, 1.0f, 0, 131072)) { burst->count = static_cast<uint32_t>((std::max)(value, 0)); changed = true; }
	}
	else if (std::holds_alternative<VfxGraphSpawnPointNode>(node.payload))
	{
		ImGui::TextDisabled("Point spawn has no parameters.");
	}
	else if (auto* spawnSphere = std::get_if<VfxGraphSpawnSphereNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Radius", &spawnSphere->radius, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* spawnBox = std::get_if<VfxGraphSpawnBoxNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("Size", &spawnBox->size.x, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* lifetime = std::get_if<VfxGraphLifetimeNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Lifetime", &lifetime->lifetime, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("Random", &lifetime->random, 0.01f, 0.0f, 120.0f) || changed;
	}
	else if (auto* initialVelocity = std::get_if<VfxGraphInitialVelocityNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("Velocity", &initialVelocity->velocity.x, 0.05f) || changed;
		changed = ImGui::DragFloat3("Velocity Random", &initialVelocity->random.x, 0.05f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("Speed", &initialVelocity->speed, 0.05f) || changed;
		changed = ImGui::DragFloat("Speed Random", &initialVelocity->speedRandom, 0.05f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* initialColor = std::get_if<VfxGraphInitialColorNode>(&node.payload))
	{
		changed = ImGui::ColorEdit4("Start", &initialColor->start.x) || changed;
		changed = ImGui::ColorEdit4("End", &initialColor->end.x) || changed;
		changed = ImGui::Checkbox("Alpha Fade", &initialColor->alphaFade) || changed;
	}
	else if (auto* initialSize = std::get_if<VfxGraphInitialSizeNode>(&node.payload))
	{
		changed = ImGui::DragFloat2("Start Size", &initialSize->start.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat2("End Size", &initialSize->end.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("Size Random", &initialSize->random, 0.01f, 0.0f, 10000.0f) || changed;
	}
	else if (auto* gravity = std::get_if<VfxGraphGravityNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("Acceleration", &gravity->acceleration.x, 0.05f) || changed;
	}
	else if (auto* drag = std::get_if<VfxGraphDragNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Damping", &drag->damping, 0.01f, 0.0f, 1000.0f) || changed;
	}
	else if (auto* initialRotation = std::get_if<VfxGraphInitialRotationNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Rotation", &initialRotation->rotation, 0.01f) || changed;
		changed = ImGui::DragFloat("Rotation Random", &initialRotation->random, 0.01f, 0.0f, 100.0f) || changed;
	}
	else if (auto* rotationRate = std::get_if<VfxGraphRotationRateNode>(&node.payload))
	{
		changed = ImGui::DragFloat("Radians / sec", &rotationRate->radiansPerSecond, 0.01f) || changed;
	}
	else if (auto* sizeOverLife = std::get_if<VfxGraphSizeOverLifeNode>(&node.payload))
	{
		const size_t oldKeyCount = sizeOverLife->multiplier.keys.size();
		DrawFloatCurveEditor(sizeOverLife->multiplier);
		changed = oldKeyCount != sizeOverLife->multiplier.keys.size() || ImGui::IsItemEdited() || changed;
	}
	else if (auto* colorOverLife = std::get_if<VfxGraphColorOverLifeNode>(&node.payload))
	{
		const size_t oldKeyCount = colorOverLife->gradient.keys.size();
		DrawColorGradientEditor(colorOverLife->gradient);
		changed = oldKeyCount != colorOverLife->gradient.keys.size() || ImGui::IsItemEdited() || changed;
	}
	else if (auto* collision = std::get_if<VfxGraphCollisionNode>(&node.payload))
	{
		static const char* kShapes[] = { "Plane", "Sphere" };
		static const char* kResponses[] = { "Bounce", "Slide", "Kill" };
		int shape = static_cast<int>(collision->shape);
		int response = static_cast<int>(collision->response);
		if (ImGui::Combo("Shape", &shape, kShapes, IM_ARRAYSIZE(kShapes))) { collision->shape = static_cast<VfxCollisionShape>(shape); changed = true; }
		if (ImGui::Combo("Response", &response, kResponses, IM_ARRAYSIZE(kResponses))) { collision->response = static_cast<VfxCollisionResponse>(response); changed = true; }
		if (collision->shape == VfxCollisionShape::Plane)
		{
			changed = ImGui::DragFloat3("Plane Normal", &collision->planeNormal.x, 0.01f) || changed;
			changed = ImGui::DragFloat("Plane Distance", &collision->planeDistance, 0.01f) || changed;
		}
		else
		{
			changed = ImGui::DragFloat3("Sphere Center", &collision->sphereCenter.x, 0.01f) || changed;
			changed = ImGui::DragFloat("Sphere Radius", &collision->sphereRadius, 0.01f, 0.001f, 10000.0f) || changed;
		}
		changed = ImGui::DragFloat("Particle Radius", &collision->particleRadius, 0.001f, 0.0f, 1000.0f) || changed;
		changed = ImGui::SliderFloat("Restitution", &collision->restitution, 0.0f, 1.0f) || changed;
		changed = ImGui::SliderFloat("Friction", &collision->friction, 0.0f, 1.0f) || changed;
		changed = ImGui::Checkbox("Generate Event", &collision->generateEvent) || changed;
	}
	else if (std::holds_alternative<VfxGraphDeathEventNode>(node.payload))
	{
		ImGui::TextDisabled("Death event producer has no parameters.");
	}
	else if (auto* subEmitter = std::get_if<VfxGraphSubEmitterNode>(&node.payload))
	{
		static const char* kEvents[] = { "Collision", "Death" };
		int eventType = static_cast<int>(subEmitter->sourceEvent);
		if (ImGui::Combo("Source Event", &eventType, kEvents, IM_ARRAYSIZE(kEvents))) { subEmitter->sourceEvent = static_cast<VfxParticleEventType>(eventType); changed = true; }
		int count = static_cast<int>(subEmitter->count);
		if (ImGui::DragInt("Child Count", &count, 1.0f, 1, static_cast<int>(VfxGraphDesc::kMaxSubEmitterSpawnCount))) { subEmitter->count = static_cast<uint32_t>(count); changed = true; }
		changed = ImGui::DragFloat("Child Lifetime", &subEmitter->lifeTime, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("Child Speed", &subEmitter->speed, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat("Spread", &subEmitter->spread, 0.01f, 0.0f, 100.0f) || changed;
		changed = ImGui::DragFloat("Inherit Velocity", &subEmitter->inheritVelocity, 0.01f) || changed;
		changed = ImGui::DragFloat2("Child Start Size", &subEmitter->startSize.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::DragFloat2("Child End Size", &subEmitter->endSize.x, 0.01f, 0.0f, 10000.0f) || changed;
		changed = ImGui::ColorEdit4("Child Start Color", &subEmitter->startColor.x) || changed;
		changed = ImGui::ColorEdit4("Child End Color", &subEmitter->endColor.x) || changed;
		changed = ImGui::Checkbox("Child Alpha Fade", &subEmitter->alphaFade) || changed;
	}
	else if (auto* spriteRenderer = std::get_if<VfxGraphSpriteRendererNode>(&node.payload))
	{
		editString("Texture", spriteRenderer->texturePath);
		changed = DrawBlendMode("Blend", spriteRenderer->blendMode) || changed;
		changed = ImGui::Checkbox("Billboard", &spriteRenderer->billboard) || changed;
	}
	else if (auto* ribbonRenderer = std::get_if<VfxGraphRibbonRendererNode>(&node.payload))
	{
		editString("Texture", ribbonRenderer->texturePath);
		changed = DrawBlendMode("Blend", ribbonRenderer->blendMode) || changed;
		changed = ImGui::DragFloat("Width", &ribbonRenderer->width, 0.001f, 0.001f, 100.0f) || changed;
		changed = ImGui::DragFloat("Length", &ribbonRenderer->length, 0.01f, 0.001f, 10000.0f) || changed;
	}
	else if (auto* trailRenderer = std::get_if<VfxGraphTrailRendererNode>(&node.payload))
	{
		editString("Texture", trailRenderer->texturePath);
		changed = DrawBlendMode("Blend", trailRenderer->blendMode) || changed;
		changed = ImGui::DragFloat("Width", &trailRenderer->width, 0.001f, 0.001f, 100.0f) || changed;
		changed = ImGui::DragFloat("Length", &trailRenderer->length, 0.01f, 0.001f, 10000.0f) || changed;
	}
	else if (auto* meshRenderer = std::get_if<VfxGraphMeshRendererNode>(&node.payload))
	{
		editString("Mesh", meshRenderer->meshPath);
		int subMesh = static_cast<int>(meshRenderer->subMeshIndex);
		if (ImGui::DragInt("Sub Mesh", &subMesh, 1.0f, 0, 1024)) { meshRenderer->subMeshIndex = static_cast<uint32_t>(subMesh); changed = true; }
		changed = DrawBlendMode("Blend", meshRenderer->blendMode) || changed;
		changed = ImGui::DragFloat3("Start Scale", &meshRenderer->startScale.x, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat3("End Scale", &meshRenderer->endScale.x, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat3("Start Rotation", &meshRenderer->startRotation.x, 0.01f) || changed;
		changed = ImGui::DragFloat3("Angular Velocity", &meshRenderer->angularVelocity.x, 0.01f) || changed;
	}
	else if (auto* fluidOutput = std::get_if<VfxGraphFluidOutputNode>(&node.payload))
	{
		static const char* kDomains[] = { "Fluid2D", "Volumetric3D" };
		int domain = static_cast<int>(fluidOutput->domain);
		if (ImGui::Combo("Domain", &domain, kDomains, IM_ARRAYSIZE(kDomains))) { fluidOutput->domain = static_cast<VfxGraphFluidDomain>(domain); changed = true; }
		changed = ImGui::DragFloat3("Local Offset", &fluidOutput->localOffset.x, 0.01f) || changed;
		changed = ImGui::DragFloat3("Source Velocity", &fluidOutput->localVelocity.x, 0.01f) || changed;
		changed = ImGui::DragFloat("Duration", &fluidOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("Radius", &fluidOutput->radius, 0.01f, 0.001f, 1000.0f) || changed;
		changed = ImGui::DragFloat("Velocity Strength", &fluidOutput->velocityStrength, 0.01f, 0.0f, 1000.0f) || changed;
		changed = ImGui::DragFloat("Density Rate", &fluidOutput->densityRate, 0.01f) || changed;
		changed = ImGui::DragFloat("Temperature Rate", &fluidOutput->temperatureRate, 0.01f) || changed;
		changed = ImGui::DragFloat("Falloff Exponent", &fluidOutput->falloffExponent, 0.01f, 0.001f, 32.0f) || changed;
		editString("Intensity Parameter", fluidOutput->intensityParameter);
		editString("Radius Parameter", fluidOutput->radiusParameter);
	}
	else if (auto* lightOutput = std::get_if<VfxGraphLightOutputNode>(&node.payload))
	{
		changed = ImGui::DragFloat3("Local Offset", &lightOutput->localOffset.x, 0.01f) || changed;
		changed = ImGui::ColorEdit3("Color", &lightOutput->color.x) || changed;
		changed = ImGui::DragFloat("Duration", &lightOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::DragFloat("Intensity", &lightOutput->intensity, 0.01f, 0.0f, 100000.0f) || changed;
		changed = ImGui::DragFloat("Range", &lightOutput->range, 0.01f, 0.001f, 10000.0f) || changed;
		editString("Intensity Parameter", lightOutput->intensityParameter);
		editString("Radius Parameter", lightOutput->radiusParameter);
	}
	else if (auto* postEffectOutput = std::get_if<VfxGraphPostEffectOutputNode>(&node.payload))
	{
		editString("Effect Name", postEffectOutput->effectName);
		changed = ImGui::DragFloat("Duration", &postEffectOutput->duration, 0.01f, 0.001f, 120.0f) || changed;
		changed = ImGui::SliderFloat("Weight", &postEffectOutput->weight, 0.0f, 1.0f) || changed;
		editString("Intensity Parameter", postEffectOutput->intensityParameter);
	}
	return changed;
#else
	(void)node;
	return false;
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawFloatCurveEditor(VfxFloatCurve& curve)
{
#ifdef USE_IMGUI
	static const char* kInterpolation[] = { "Linear", "Step", "SmoothStep" };
	int interpolation = static_cast<int>(curve.interpolation);
	if (ImGui::Combo("Interpolation", &interpolation, kInterpolation, IM_ARRAYSIZE(kInterpolation)))
	{
		curve.interpolation = static_cast<VfxCurveInterpolation>(interpolation);
		MarkGraphDirty();
	}

	int removeIndex = -1;
	for (size_t i = 0; i < curve.keys.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		bool changed = ImGui::DragFloat("Time", &curve.keys[i].time, 0.01f, 0.0f, 1.0f);
		changed = ImGui::DragFloat("Value", &curve.keys[i].value, 0.01f) || changed;
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove"))
		{
			removeIndex = static_cast<int>(i);
		}
		if (changed)
		{
			MarkGraphDirty();
		}
		ImGui::PopID();
	}
	if (removeIndex >= 0 && curve.keys.size() > 1u)
	{
		curve.keys.erase(curve.keys.begin() + removeIndex);
		MarkGraphDirty();
	}
	if (curve.keys.size() < VfxGraphDesc::kMaxCurveKeys && ImGui::Button("Add Curve Key"))
	{
		curve.keys.push_back({ 1.0f, 1.0f });
		MarkGraphDirty();
	}
	std::sort(curve.keys.begin(), curve.keys.end(), [](const VfxFloatCurveKey& a, const VfxFloatCurveKey& b) { return a.time < b.time; });
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawColorGradientEditor(VfxColorGradient& gradient)
{
#ifdef USE_IMGUI
	static const char* kInterpolation[] = { "Linear", "Step", "SmoothStep" };
	int interpolation = static_cast<int>(gradient.interpolation);
	if (ImGui::Combo("Interpolation", &interpolation, kInterpolation, IM_ARRAYSIZE(kInterpolation)))
	{
		gradient.interpolation = static_cast<VfxCurveInterpolation>(interpolation);
		MarkGraphDirty();
	}

	int removeIndex = -1;
	for (size_t i = 0; i < gradient.keys.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		bool changed = ImGui::DragFloat("Time", &gradient.keys[i].time, 0.01f, 0.0f, 1.0f);
		changed = ImGui::ColorEdit4("Color", &gradient.keys[i].color.x) || changed;
		ImGui::SameLine();
		if (ImGui::SmallButton("Remove"))
		{
			removeIndex = static_cast<int>(i);
		}
		if (changed)
		{
			MarkGraphDirty();
		}
		ImGui::PopID();
	}
	if (removeIndex >= 0 && gradient.keys.size() > 1u)
	{
		gradient.keys.erase(gradient.keys.begin() + removeIndex);
		MarkGraphDirty();
	}
	if (gradient.keys.size() < VfxGraphDesc::kMaxGradientKeys && ImGui::Button("Add Gradient Key"))
	{
		gradient.keys.push_back({ 1.0f, { 1.0f, 1.0f, 1.0f, 1.0f } });
		MarkGraphDirty();
	}
	std::sort(gradient.keys.begin(), gradient.keys.end(), [](const VfxColorGradientKey& a, const VfxColorGradientKey& b) { return a.time < b.time; });
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawPreviewPanel()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Preview");
	ImGui::DragFloat3("Preview Position", &previewPosition_.x, 0.05f);
	if (!previewHandle_.IsValid())
	{
		if (ImGui::Button("Play Preview"))
		{
			StartPreview();
		}
	}
	else
	{
		if (ImGui::Button("Restart Preview"))
		{
			StopPreview();
			StartPreview();
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Preview"))
		{
			StopPreview();
			lastMessage_ = "Preview stopped.";
		}
		VfxGraphRuntime::GetInstance()->SetLoopPosition(previewHandle_, previewPosition_);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Preview uses Phase13 particles plus Phase18 Fluid/Light/PostEffect adapters through the Phase26 graph runtime.");
#endif // USE_IMGUI
}

void VfxGraphEditor::DrawCompileDiagnostics()
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Compiler Diagnostics");
	if (compileResult_.success)
	{
		ImGui::Text("Compile: OK | emitters=%d | integrations=%d | warnings=%d", static_cast<int>(compileResult_.program.emitters.size()), static_cast<int>(compileResult_.program.integrationOneShotCue.tracks.size()), static_cast<int>(compileResult_.warnings.size()));
	}
	else
	{
		ImGui::Text("Compile: FAILED | errors=%d", static_cast<int>(compileResult_.errors.size()));
	}
	for (const auto& warning : compileResult_.warnings)
	{
		ImGui::BulletText("Warning: %s", warning.c_str());
	}
	for (const auto& error : compileResult_.errors)
	{
		ImGui::BulletText("Error: %s", error.c_str());
	}

	if (VfxGraphEmitterDesc* emitter = GetSelectedEmitter())
	{
		for (const auto& compiledEmitter : compileResult_.program.emitters)
		{
			if (compiledEmitter.name != emitter->name)
			{
				continue;
			}
			ImGui::Text("Execution Order:");
			for (uint32_t id : compiledEmitter.executionOrder)
			{
				ImGui::SameLine();
				ImGui::Text("%u", id);
			}
			break;
		}
	}
#endif // USE_IMGUI
}

} // namespace Ken4lowEngine
